// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#if HAVE_PF_ROUTE

#include "interface_monitor.h"
#include "sys_socket.h"
#include <sys/sockio.h>
#if HAVE_SYSCTL_PF_ROUTE
    #include <sys/sysctl.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>

using namespace asio::generic;

static inline auto alignedRtMessageLength(const sockaddr * sa) -> size_t {
    constexpr size_t salign = (alignof(rt_msghdr) - 1);
    #if HAVE_SOCKADDR_SA_LEN
        return sa->sa_len ? ((sa->sa_len + salign) & ~salign) : (salign + 1);
    #else
        size_t len;
        if (sa->sa_family == AF_INET)
            len = sizeof(sockaddr_in);
        else if (sa->sa_family == AF_INET6)
            len = sizeof(sockaddr_in6);
        else if (sa->sa_family == AF_LINK)
            len = sizeof(sockaddr_dl);
        else  {
            WSDLOG_ERROR("Unexpected address family {} if PF_ROUTE message", sa->sa_family);
            std::terminate();
        }
        return (len + salign) & ~salign;
    #endif
}

class InterfaceMonitorImpl : public InterfaceMonitor {

private:
    struct ParseResult {
        std::optional<ip::address> addr;
        std::optional<NetworkInterface> iface;
    };
public:
    InterfaceMonitorImpl(asio::io_context & ctxt,  const refcnt_ptr<Config> & config):
        m_config(config),
        m_socket(ctxt, raw_protocol(PF_ROUTE, AF_UNSPEC)),
        m_recvBuffer(m_config->pageSize()) {
    }

    void start(Handler & handler) override {

        m_handler = &handler;
        
        WSDLOG_INFO("Starting interface monitor");
    
        read();

        loadInitial();
    }
    
    void stop() override {
        WSDLOG_INFO("Stopping interface monitor");
        m_handler = nullptr;
        m_socket.close();
    }

private:
    #if HAVE_SYSCTL_PF_ROUTE

    void loadInitial() {
        int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
        std::vector<std::byte> buf;
        size_t tableSize = 0;
        for (; ; ) {
            int res = sysctl(mib, std::size(mib), buf.data(), &tableSize, nullptr, 0);
            if (res != 0) {
                if (int err = errno; err != ENOMEM)
                    ptl::throwErrorCode(err, "sysctl(CTL_NET, PF_ROUTE)");
            } else {
                if (buf.size() >= tableSize)
                    break;
            }

            buf.resize(tableSize);
        }

        parseTable(buf.data(), buf.data() + tableSize, /*sequential*/true);
    }

    #elif HAVE_SIOCGLIFCONF

    void loadInitial() {
        std::vector<lifreq> buf(256); //no machine is expected to have more than 256 interfaces :)
        auto count = ioctlSocket<GetLInterfaceConf>(m_socket, AF_UNSPEC, buf.data(), buf.size()).value();
        for (size_t i = 0; i < count; ++i) {
            auto & req = buf[i];
            ip::address addr;
            if (req.lifr_addr.ss_family == AF_INET) {
                if (!m_config->enableIPv4())
                    continue;
                auto addr4 = (const sockaddr_in *)&req.lifr_addr;
                addr = makeAddress(*addr4);
            } else if (req.lifr_addr.ss_family == AF_INET6) {
                if (!m_config->enableIPv6())
                    continue;
                auto addr6 = (const sockaddr_in6 *)&req.lifr_addr;
                auto cppAddr = makeAddress(*addr6);
                if (!cppAddr.is_link_local()) 
                    continue;    
                addr = cppAddr;
            } else {
                continue;
            }
            
            sys_string name(req.lifr_name);
            
            auto interfaceFlags = ioctlSocket<GetLInterfaceFlags>(m_socket, name).value();
            if ((interfaceFlags & IFF_LOOPBACK) || !(interfaceFlags & IFF_MULTICAST))
                continue;
            auto index = ioctlSocket<GetLInterfaceIndex>(m_socket, name).value();
            m_handler->addAddress(NetworkInterface(index, name), addr);
        }
    }

    #endif

    void read() {

        asio::mutable_buffer buffer(m_recvBuffer.data() + m_recvOffset, m_recvBuffer.size() - m_recvOffset);
        m_socket.async_receive(buffer,
            [this, holder = refcnt_retain(this)](const asio::error_code & ec, size_t bytesRead){

            if (!m_handler)
                return;
            
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    WSDLOG_CRITICAL("error reading from route socket {}", ec.message());
                    m_handler->onFatalInterfaceMonitorError(ec);
                }
                
                return;
            }

            auto end = m_recvBuffer.data() + m_recvOffset + bytesRead;
            auto last = parseTable(m_recvBuffer.data(), end, /*sequential*/false);
            memcpy(m_recvBuffer.data(), last, end - last);
            m_recvOffset = end - last;

            read();
        });
    }

    auto parseTable(const std::byte * first, const std::byte * last, bool sequential) ->  const std::byte * {
        
        std::unordered_map<int, bool> knownIfaces;
        std::optional<NetworkInterface> lastInterface;
        for( ; size_t(last - first) > sizeof(rt_msghdr::rtm_msglen); first += ((const rt_msghdr *)first)->rtm_msglen) {
            
            auto header = (const rt_msghdr *)first;
            if (header->rtm_msglen > size_t(last - first))
                break;
            
            const std::byte * addrLast = first + header->rtm_msglen;

            const std::byte * addrFirst;
            int entryMask;
            int interfaceFlags = 0; //stupid gcc misdetects "not initialized"
            switch(header->rtm_type) {
            case RTM_NEWADDR:
            case RTM_DELADDR: {
                auto * ifaHeader = (const ifa_msghdr *)header;
                addrFirst = first + sizeof(ifa_msghdr);
                entryMask = ifaHeader->ifam_addrs;
                break;
            }
            case RTM_IFINFO: {
                auto * ifHeader = (const if_msghdr *)header;
                addrFirst = first + sizeof(if_msghdr);
                entryMask = ifHeader->ifm_addrs;
                interfaceFlags = ifHeader->ifm_flags;
                break;
            }
            default:
                continue;
            }

            auto result = parseAddresses(addrFirst, addrLast, entryMask);
            if (sequential) {
                if (!result.iface)
                    result.iface = lastInterface;
                else
                    lastInterface = result.iface;
            }
            
            if (result.iface) {
                
                if (!m_config->isAllowedInterface(result.iface->name)) {
                    WSDLOG_DEBUG("Interface {} is not allowed in configuration - ignoring", *result.iface);
                    continue;
                }
                
                if (header->rtm_type == RTM_IFINFO) {
                    knownIfaces[result.iface->index] = (interfaceFlags & IFF_LOOPBACK) || !(interfaceFlags & IFF_MULTICAST);
                } else {
                    if (auto it = knownIfaces.find(result.iface->index); it == knownIfaces.end()) {
                        #if HAVE_SIOCGLIFCONF
                            using GetInterfaceFlagsType = GetLInterfaceFlags;
                        #else
                            using GetInterfaceFlagsType = GetInterfaceFlags;
                        #endif
                        if (auto flagsRes = ioctlSocket<GetInterfaceFlagsType>(m_socket, result.iface->name)) {
                            interfaceFlags = flagsRes.assume_value();
                            bool ignore = (interfaceFlags & IFF_LOOPBACK) || !(interfaceFlags & IFF_MULTICAST);
                            knownIfaces.emplace(result.iface->index, ignore);
                        }
                    }
                }
            }
            
            if (!result.addr || !result.iface)
                continue;
            
            handleDetected(header->rtm_type != RTM_DELADDR, *result.iface, *result.addr, knownIfaces);
            
        }
        return first;
    }

    auto parseAddresses(const std::byte * first, const std::byte * last, int entryMask) -> ParseResult {

        ParseResult result;
        if (size_t(last - first) >= sizeof(sockaddr)) {
            for(int entryBit = 1; entryBit <= entryMask; entryBit <<= 1) {
                if (entryBit & entryMask) {
                    auto addr = (const sockaddr *)first;
                    switch(entryBit) {
                        case RTA_IFA:
                            if (addr->sa_family == AF_INET) {
                                if (m_config->enableIPv4()) {
                                    auto addr4 = (const sockaddr_in *)addr;
                                    result.addr.emplace(makeAddress(*addr4));
                                }
                            } else if (addr->sa_family == AF_INET6) {
                                if (m_config->enableIPv6()) {
                                    auto addr6 = (const sockaddr_in6 *)addr;
                                    auto cppAddr = makeAddress(*addr6);
                                    if (cppAddr.is_link_local()) 
                                        result.addr.emplace(cppAddr);
                                }
                            }
                            break;
                        case RTA_IFP:
                            if (addr->sa_family == AF_LINK) {
                                auto addrDl = (const sockaddr_dl *)addr;
                                result.iface.emplace(addrDl->sdl_index, addrDl->sdl_data, addrDl->sdl_data + addrDl->sdl_nlen);
                            }
                            break;
                    }
                    first += alignedRtMessageLength(addr);
                    if (size_t(last - first) < sizeof(sockaddr))
                        break;
                }
            }
        }
        //On Illumos PF_ROUTE does not return scope for link local addresses
        #ifdef __sun
        if (result.addr && result.addr->is_v6() && result.iface) {
            ip::address_v6 addr6 = result.addr->to_v6();
            if (addr6.scope_id() == 0) {
                addr6.scope_id(result.iface->index);
                *result.addr = addr6;
            }
        }
        #endif
        return result;
    }
    
    void handleDetected(bool isAdded, NetworkInterface iface, ip::address addr, const std::unordered_map<int, bool> & knownIfaces) {
        
        if (isAdded) {
            
            bool ignore = true;
            if (auto it = knownIfaces.find(iface.index); it != knownIfaces.end()) {
                ignore = it->second;
            }
            
            if (!ignore)
                m_handler->addAddress(iface, addr);
            else
                WSDLOG_DEBUG("Interface {} is loopback or doesn't support multicast - ignoring", iface);
            
        } else {
            
            m_handler->removeAddress(iface, addr);
        }
    }

private:
    const refcnt_ptr<Config> m_config;
    Handler * m_handler = nullptr;

    raw_protocol::socket m_socket;
    std::vector<std::byte> m_recvBuffer;
    size_t m_recvOffset = 0;
};

auto createInterfaceMonitor(asio::io_context & ctxt, const refcnt_ptr<Config> & config) -> refcnt_ptr<InterfaceMonitor> {
    return refcnt_attach(new InterfaceMonitorImpl(ctxt, config));
}

#endif
