// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "udp_server.h"
#include "sys_socket.h"
#include "exc_handling.h"

#if defined(IP_RECVIF)
    #include <net/if_dl.h>
#endif

static constexpr size_t g_wsdMaxDatagramLength = 32767;

class UdpServerImpl : public UdpServer {
public:
    UdpServerImpl(asio::io_context & ctxt,
                  const refcnt_ptr<Config> & config,
                  const NetworkInterface & iface,
                  const ip::address & addr):
        m_config(config),
        m_iface(iface),
        m_recvSocket(ctxt),
        m_multicastSendSocket(ctxt),
        m_unicastSendSocket(ctxt),
        m_recvBuffer(g_wsdMaxDatagramLength),
        m_isV4(addr.is_v4()) {

        auto prot = m_isV4 ? ip::udp::v4() : ip::udp::v6();

        m_recvSocket.open(prot);
        m_multicastSendSocket.open(prot);
        m_unicastSendSocket.open(prot);
        
        m_recvSocket.non_blocking();

        m_recvSocket.set_option(ip::udp::socket::reuse_address(true));
        m_unicastSendSocket.set_option(ip::udp::socket::reuse_address(true));

        if (m_isV4)
            initAddresses(addr.to_v4(), iface);
        else
            initAddresses(addr.to_v6(), iface);

    }

    void start(Handler & handler) override {
        m_handler = &handler;
        read();
        WSDLOG_INFO("Starting UDP server on {}", m_iface);
    }
    
    void stop() override {
        m_handler = nullptr;
        m_recvSocket.close();
        m_multicastSendSocket.close();
        m_unicastSendSocket.close();
        WSDLOG_INFO("Stopping UDP server on {}", m_iface);
    }
    
     void broadcast(XmlCharBuffer && data, std::function<void (asio::error_code)> continuation) override {
         write(std::move(data), &UdpServerImpl::m_multicastSendSocket, m_multicastDest, false, continuation);
     }

private:
    
    ~UdpServerImpl() noexcept {
    }

    void initAddresses(const ip::address_v4 & addr, [[maybe_unused]] const NetworkInterface & iface) {
        
        auto multicastGroupAddress = ip::make_address_v4(g_WsdMulticastGroupV4);
        
        m_multicastDest = ip::udp::endpoint(multicastGroupAddress, g_WsdUdpPort);

    #if PTL_HAVE_IP_MREQN
        ip_mreqn multicastGroupRequest;
        multicastGroupRequest.imr_address.s_addr = htonl(addr.to_uint());
        multicastGroupRequest.imr_ifindex = iface.index;
    #else
        ip_mreq multicastGroupRequest;
        multicastGroupRequest.imr_interface.s_addr = htonl(addr.to_uint());
    #endif
        multicastGroupRequest.imr_multiaddr.s_addr = htonl(multicastGroupAddress.to_uint());

        setSocketOption(m_recvSocket, ptl::SockOptIPv4AddMembership, multicastGroupRequest);

        #if defined(__linux__)
            setSocketOption(m_recvSocket, ptl::SockOptIPv4MulticastAll, false);
        #endif
        
        ReadMessageControl::applyV4(m_recvSocket);
            
        m_recvSocket.bind(ip::udp::endpoint(multicastGroupAddress, g_WsdUdpPort));
        m_unicastSendSocket.bind(ip::udp::endpoint(addr, g_WsdUdpPort));
        
    #if !defined(__NetBSD__) && !defined(__sun)
        setSocketOption(m_multicastSendSocket, ptl::SockOptIPv4MulticastIface, multicastGroupRequest);
    #else
        setSocketOption(m_multicastSendSocket, ptl::SockOptIPv4MulticastIface, multicastGroupRequest.imr_interface);
    #endif
        setSocketOption(m_multicastSendSocket, ptl::SockOptIPv4MulticastLoop, false);
        setSocketOption(m_multicastSendSocket, ptl::SockOptIPv4MulticastTtl, uint8_t(m_config->hopLimit()));

        if (m_config->sourcePort() != 0)
            m_multicastSendSocket.bind(ip::udp::endpoint(addr, m_config->sourcePort()));
    }

    void initAddresses(const ip::address_v6 & addr, const NetworkInterface & iface) {
        
        auto multicastGroupAddress = ip::make_address_v6(g_WsdMulticastGroupV6);
        
        auto destAddr = multicastGroupAddress;
        destAddr.scope_id(iface.index);
        m_multicastDest = ip::udp::endpoint(destAddr, g_WsdUdpPort);
        
        m_recvSocket.set_option(ip::multicast::join_group(multicastGroupAddress, iface.index));
        m_recvSocket.set_option(ip::v6_only(true));

        #ifdef __linux__
            setSocketOption(m_recvSocket, ptl::SockOptIPv6MulticastAll, false);
        #endif

        m_recvSocket.bind(ip::udp::endpoint(ip::address_v6(multicastGroupAddress.to_bytes(), iface.index), g_WsdUdpPort));
        m_unicastSendSocket.bind(ip::udp::endpoint(ip::address_v6(addr.to_bytes(), iface.index), g_WsdUdpPort));

        setSocketOption(m_multicastSendSocket, ptl::SockOptIPv6MulticastLoop, false);
        m_multicastSendSocket.set_option(ip::multicast::hops(m_config->hopLimit()));
        setSocketOption(m_multicastSendSocket, ptl::SockOptIPv6MulticastIface, iface.index);

        if (m_config->sourcePort() != 0)
            m_multicastSendSocket.bind(ip::udp::endpoint(ip::udp::endpoint(ip::address_v6(addr.to_bytes(), iface.index), m_config->sourcePort())));

    }
    
#if !defined(__linux__) && defined(IP_RECVIF)
    class ReadMessageControl {
    private:
        alignas(cmsghdr) uint8_t m_data[sizeof(cmsghdr) + CMSG_SPACE(sizeof(sockaddr_dl))];
    public:
        static constexpr size_t size() noexcept { return sizeof(m_data); }
        cmsghdr * data() noexcept { return reinterpret_cast<cmsghdr *>(m_data); }
        
        static bool checkInterfaceIndexV4(msghdr & msg, int ifIndex) {
            if (msg.msg_flags & MSG_CTRUNC) {
                WSDLOG_ERROR("UDP server on {}, control info is truncated", ifIndex);
                return true;
            }
            
            if (msg.msg_controllen < sizeof(struct cmsghdr))
                return true;
            
            for (struct cmsghdr * cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr)) {
                if (cmptr->cmsg_level == IPPROTO_IP && cmptr->cmsg_type == IP_RECVIF) {
                    auto sdl = (struct sockaddr_dl *)CMSG_DATA(cmptr);
                    return sdl->sdl_index == ifIndex;
                }
            }
            
            return true;
        }
        
        static void applyV4(ip::udp::socket & sock) {
            int val = 1;
            ptl::setSocketOption(sock, IPPROTO_IP, IP_RECVIF, &val, sizeof(val));
        }
    };
#else
    class ReadMessageControl {
    public:
        static constexpr size_t size() noexcept { return 0; }
        static cmsghdr * data() const noexcept { return nullptr; }
        
        static bool checkInterfaceIndexV4(msghdr & /*msg*/, int /*ifIndex*/) { return true; }
        
        static void applyV4(ip::udp::socket & /*sock*/) {}
    };
#endif

    void read() {
        m_recvSocket.async_receive_from(asio::null_buffers(), m_recvSender,
            [this, holder = refcnt_retain(this)](asio::error_code ec, size_t bytesRecvd) {

            if (!m_handler)
                return;
            
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    WSDLOG_ERROR("UDP server on {}, error reading: {}", m_iface, ec.message());
                    m_handler->onFatalUdpError();
                }
                
                return;
            }
            
            sockaddr_storage from{};
            
            iovec iov[] = {{m_recvBuffer.data(), m_recvBuffer.size()}};
            
            ReadMessageControl control;
            
            msghdr msg{};
            msg.msg_name = &from;
            msg.msg_namelen = sizeof(from);
            msg.msg_iov = iov;
            msg.msg_iovlen = std::size(iov);
            msg.msg_control = control.data();
            msg.msg_controllen = control.size();
            
            for ( ; ; ) {
                bytesRecvd = ptl::receiveSocket(m_recvSocket, &msg, 0, ec);
                if (!ec)
                    break;
                if (ec == std::errc::interrupted)
                    continue;
                if (ec == std::errc::operation_would_block || ec == std::errc::resource_unavailable_try_again)
                    return;
                    
                WSDLOG_ERROR("UDP server on {}, error reading: {}", m_iface, ec.message());
                m_handler->onFatalUdpError();
                return;
            }
            
            if (from.ss_family == AF_INET) {
                auto from4 = (const sockaddr_in *)&from;
                m_recvSender = ip::udp::endpoint(makeAddress(*from4), ntohs(from4->sin_port));
            } else if (from.ss_family == AF_INET6) {
                auto from6 = (const sockaddr_in6 *)&from;
                m_recvSender = ip::udp::endpoint(makeAddress(*from6), ntohs(from6->sin6_port));
            } else {
                WSDLOG_DEBUG("UDP on {}, received invalid source address, ignoring", m_iface);
                read();
                return;
            }
            
            if (msg.msg_flags & MSG_TRUNC)
                WSDLOG_ERROR("UDP server on {}, read data truncated", m_iface);
            
            if (m_isV4 && !ReadMessageControl::checkInterfaceIndexV4(msg, m_iface.index)) {
                read();
                return;
            }

            if (spdlog::should_log(spdlog::level::trace))
                WSDLOG_TRACE("UDP on {}, received from {}:{}: {}", m_iface, m_recvSender.address().to_string(), m_recvSender.port(),
                              std::string_view((const char *)m_recvBuffer.data(), bytesRecvd));
            else
                WSDLOG_DEBUG("UDP on {}, received {} bytes from {}:{}", m_iface, bytesRecvd, m_recvSender.address().to_string(), m_recvSender.port());

            std::optional<XmlCharBuffer> maybeReply;
            try {
                auto doc = XmlDoc::parseMemory(m_recvBuffer.data(), int(bytesRecvd));
                maybeReply = m_handler->handleUdpRequest(std::move(doc));
            } catch (std::exception & ex) {
                WSDLOG_ERROR("UDP on {}, error handling request: {}", m_iface, ex.what());
                WSDLOG_TRACE("{}", formatCaughtExceptionBacktrace());
            }

            if (maybeReply)
                write(std::move(*maybeReply), &UdpServerImpl::m_unicastSendSocket, m_recvSender, true);
            read();
        });
    }

    void write(XmlCharBuffer && data, ip::udp::socket UdpServerImpl::*socketPtr, ip::udp::endpoint dest,
               bool isUnicast, std::function<void (asio::error_code)> continuation = nullptr) {
        int repeatCount = (socketPtr == &UdpServerImpl::m_multicastSendSocket ? 4 : 2);
        RefCountedContainerBuffer buffer(std::move(data));
        auto & socket = this->*socketPtr;

        struct Callback {
            refcnt_ptr<UdpServerImpl> me;
            RefCountedContainerBuffer<XmlCharBuffer> buffer;
            ip::udp::socket & socket;
            ip::udp::endpoint dest;
            bool isUnicast;
            int repeatCount;
            std::function<void (asio::error_code)> continuation;

            void operator()(asio::error_code ec, size_t /*bytesSent, ip::udp::socket * socket*/) {
                
                if (!me->m_handler)
                    return;
                
            #ifdef __OpenBSD__
                //On OpenBSD unicast send_to can fail with EACCESS when firewall
                //blocks it. This isn't fatal and shouldn't be an error at all, so let's
                //log it and treat it as success
                if (isUnicast && ec == asio::error::access_denied) {
                    WSDLOG_DEBUG("UDP server on {}, error writing: blocked by firewall", me->m_iface);
                    ec = asio::error_code{};
                }
            #endif

                if (ec) {
                    if (ec != asio::error::operation_aborted) {
                        WSDLOG_ERROR("UDP server on {}, error writing: {}", me->m_iface, ec.message());
                        
                        if (continuation)
                            continuation(ec);
                        else
                            me->m_handler->onFatalUdpError();
                    }
                    return;
                }

                if (--repeatCount == 0) {
                    if (continuation)
                        continuation(ec);
                    return;
                }

                std::uniform_int_distribution<> distrib(50, 250);
                auto delay = distrib(g_Random);

                auto timer = std::make_shared<asio::steady_timer>(socket.get_executor(), asio::chrono::milliseconds(delay));
                
                timer->async_wait([timer, *this](const asio::error_code & ec) {
                    if (ec || !socket.is_open())
                        return;
                    
                    socket.async_send_to(buffer, dest, Callback{*this});
                });
            }
        };

        if (spdlog::should_log(spdlog::level::trace))
            WSDLOG_TRACE("UDP on {}, sending to {}:{}: {}", m_iface, dest.address().to_string(), m_recvSender.port(),
                          std::string_view((const char *)buffer.begin()->data(), buffer.begin()->size()));
        else
            WSDLOG_DEBUG("UDP on {}, sending {} bytes to {}:{}", m_iface, buffer.begin()->size(), dest.address().to_string(), m_recvSender.port());
        
        socket.async_send_to(buffer, dest, Callback{refcnt_retain(this), buffer, socket, dest, isUnicast, repeatCount, continuation});
    }

private:
    const refcnt_ptr<Config> m_config;
    NetworkInterface m_iface;
    Handler * m_handler = nullptr;

    ip::udp::socket m_recvSocket;
    ip::udp::socket m_multicastSendSocket;
    ip::udp::socket m_unicastSendSocket;

    ip::udp::endpoint m_multicastDest;
    std::vector<std::byte> m_recvBuffer;
    ip::udp::endpoint m_recvSender;

    bool m_isV4;
};

refcnt_ptr<UdpServer> createUdpServer(asio::io_context & ctxt,
                                      const refcnt_ptr<Config> & config,
                                      const NetworkInterface & iface,
                                      const ip::address & addr) {

    return refcnt_attach(new UdpServerImpl(ctxt, config, iface, addr));
    
}
