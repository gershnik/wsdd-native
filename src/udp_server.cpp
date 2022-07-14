// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "udp_server.h"
#include "sys_socket.h"
#include "exc_handling.h"

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
        m_recvBuffer(g_wsdMaxDatagramLength) {

        auto prot = addr.is_v4() ? ip::udp::v4() : ip::udp::v6();

        m_recvSocket.open(prot);
        m_multicastSendSocket.open(prot);
        m_unicastSendSocket.open(prot);

        m_recvSocket.set_option(ip::udp::socket::reuse_address(true));
        m_unicastSendSocket.set_option(ip::udp::socket::reuse_address(true));

        if (addr.is_v4()) 
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
         write(std::move(data), &UdpServerImpl::m_multicastSendSocket, m_multicastDest, continuation);
     }

private:
    
    ~UdpServerImpl() noexcept {
    }

    void initAddresses(const ip::address_v4 & addr, const NetworkInterface & iface) {
        
        auto multicastGroupAddress = ip::make_address_v4(g_WsdMulticastGroupV4);
        
        m_multicastDest = ip::udp::endpoint(multicastGroupAddress, g_WsdUdpPort);

        ip_mreqn multicastGroupRequest;
        multicastGroupRequest.imr_multiaddr.s_addr = htonl(multicastGroupAddress.to_uint());
        multicastGroupRequest.imr_address.s_addr = htonl(addr.to_uint());
        multicastGroupRequest.imr_ifindex = iface.index;

        setSocketOption(m_recvSocket, SockOptIpAddMembershipRef{&multicastGroupRequest});

        #ifdef __linux__
            setSocketOption(m_recvSocket, SockOptIpMulticastAll{false});
        #endif

        m_recvSocket.bind(ip::udp::endpoint(multicastGroupAddress, g_WsdUdpPort));
        m_unicastSendSocket.bind(ip::udp::endpoint(addr, g_WsdUdpPort));

        setSocketOption(m_multicastSendSocket, SockOptIpMulticastInterfaceRef{&multicastGroupRequest});
        m_multicastSendSocket.set_option(ip::multicast::enable_loopback(false));
        m_multicastSendSocket.set_option(ip::multicast::hops(m_config->hopLimit()));

    }

    void initAddresses(const ip::address_v6 & addr, const NetworkInterface & iface) {
        
        auto multicastGroupAddress = ip::make_address_v6(g_WsdMulticastGroupV6);
        
        auto destAddr = multicastGroupAddress;
        destAddr.scope_id(iface.index);
        m_multicastDest = ip::udp::endpoint(destAddr, g_WsdUdpPort);
        
        m_recvSocket.set_option(ip::multicast::join_group(multicastGroupAddress, iface.index));
        m_recvSocket.set_option(ip::v6_only(true));

        #ifdef __linux__
            setSocketOption(m_recvSocket, SockOptIpv6MulticastAll{false});
        #endif

        m_recvSocket.bind(ip::udp::endpoint(ip::address_v6(multicastGroupAddress.to_bytes(), iface.index), g_WsdUdpPort));
        m_unicastSendSocket.bind(ip::udp::endpoint(ip::address_v6(addr.to_bytes(), iface.index), g_WsdUdpPort));

        m_multicastSendSocket.set_option(ip::multicast::enable_loopback(false));
        m_multicastSendSocket.set_option(ip::multicast::hops(m_config->hopLimit()));
        setSocketOption(m_multicastSendSocket, SockOptIpv6MulticastInterface{iface.index});
    }

    void read() {
        asio::mutable_buffer buffer(m_recvBuffer.data(), m_recvBuffer.size());

        m_recvSocket.async_receive_from(buffer, m_recvSender, 
            [this, holder = refcnt_retain(this)](const asio::error_code & ec, size_t bytesRecvd) {

            if (!m_handler)
                return;
            
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    WSDLOG_ERROR("UDP server on {}, error reading: {}", m_iface, ec.message());
                    m_handler->onFatalUdpError();
                }
                
                return;
            }

            if (spdlog::should_log(spdlog::level::trace))
                WSDLOG_TRACE("UDP on {}, received from {}: {}", m_iface, m_recvSender.address().to_string(),
                              std::string_view((const char *)m_recvBuffer.data(), bytesRecvd));
            else
                WSDLOG_DEBUG("UDP on {}, received {} bytes from {}", m_iface, bytesRecvd, m_recvSender.address().to_string());

            std::optional<XmlCharBuffer> maybeReply;
            try {
                auto doc = XmlDoc::parseMemory(m_recvBuffer.data(), int(bytesRecvd));
                maybeReply = m_handler->handleUdpRequest(std::move(doc));
            } catch (std::exception & ex) {
                WSDLOG_ERROR("UDP on {}, error handling request: {}", m_iface, ex.what());
                WSDLOG_TRACE("{}", formatCaughtExceptionBacktrace());
            }

            if (maybeReply)
                write(std::move(*maybeReply), &UdpServerImpl::m_unicastSendSocket, m_recvSender);
            read();
        });
    }

    void write(XmlCharBuffer && data, ip::udp::socket UdpServerImpl::*socketPtr, ip::udp::endpoint dest,
               std::function<void (asio::error_code)> continuation = nullptr) {
        int repeatCount = (socketPtr == &UdpServerImpl::m_multicastSendSocket ? 4 : 2);
        RefCountedContainerBuffer buffer(std::move(data));
        auto & socket = this->*socketPtr;

        struct Callback {
            refcnt_ptr<UdpServerImpl> me;
            RefCountedContainerBuffer<XmlCharBuffer> buffer;
            ip::udp::socket & socket;
            ip::udp::endpoint dest;
            int repeatCount;
            std::function<void (asio::error_code)> continuation;

            void operator()(const asio::error_code & ec, size_t /*bytesSent, ip::udp::socket * socket*/) {
                
                if (!me->m_handler)
                    return;
                
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
            WSDLOG_TRACE("UDP on {}, sending to {}: {}", m_iface, dest.address().to_string(),
                          std::string_view((const char *)buffer.begin()->data(), buffer.begin()->size()));
        else
            WSDLOG_DEBUG("UDP on {}, sending {} bytes to {}", m_iface, buffer.begin()->size(), dest.address().to_string());
        
        socket.async_send_to(buffer, dest, Callback{refcnt_retain(this), buffer, socket, dest, repeatCount, continuation});
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

};

refcnt_ptr<UdpServer> createUdpServer(asio::io_context & ctxt, 
                                      const refcnt_ptr<Config> & config,
                                      const NetworkInterface & iface,
                                      const ip::address & addr) {

    return refcnt_attach(new UdpServerImpl(ctxt, config, iface, addr));
    
}
