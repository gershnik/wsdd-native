// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#if HAVE_PF_ROUTE

#include "interface_monitor.h"
#include "sys_socket.h"

#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>

using namespace asio::generic;

#define    IN6_IS_SCOPE_LINKLOCAL(a)    \
    ((IN6_IS_ADDR_LINKLOCAL(a)) ||    \
    (IN6_IS_ADDR_MC_LINKLOCAL(a)))


static inline auto alignedRtMessageLength(const sockaddr * sa) -> size_t {
    constexpr size_t salign = (alignof(rt_msghdr) - 1);
    return sa->sa_len ? ((sa->sa_len + salign) & ~salign) : (salign + 1);
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
    
    void stop() override {
        WSDLOG_INFO("Stopping interface monitor");
        m_handler = nullptr;
        m_socket.close();
    }

private:

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
            int interfaceFlags;
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
                        if (auto flagsRes = ioctlSocket<GetInterfaceFlags>(m_socket, result.iface->name)) {
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
                                    result.addr.emplace(ip::address_v4(ntohl(addr4->sin_addr.s_addr)));
                                }
                            } else if (addr->sa_family == AF_INET6) {
                                if (m_config->enableIPv6()) {
                                    auto addr6 = (const sockaddr_in6 *)addr;
                                    union {
                                        ip::address_v6::bytes_type asio;
                                        in6_addr raw;
                                    } clearAddr;
                                    memcpy(&clearAddr.raw, addr6->sin6_addr.s6_addr, sizeof(clearAddr.raw));
                                    uint32_t scope = addr6->sin6_scope_id;
                                    if (IN6_IS_SCOPE_LINKLOCAL(&clearAddr.raw)) {
                                        if (uint32_t embeddedScope = htons(clearAddr.raw.__u6_addr.__u6_addr16[1]))
                                            scope = embeddedScope;
                                        clearAddr.raw.__u6_addr.__u6_addr16[1] = 0;
                                    }
                                    ip::address_v6 cppAddr(clearAddr.asio, scope);
                                    if (cppAddr.is_link_local()) //yes in practice this duplicates the above check but it is cleaner
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
