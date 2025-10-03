// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#if HAVE_NETLINK

#include "interface_monitor.h"
#include "sys_socket.h"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <asm/types.h>

using namespace asio::generic;

class InterfaceMonitorImpl : public InterfaceMonitor {

private:
    enum ParseStatus {
        Done,
        ExpectMore,
        BufferTooSmall,
        Error
    };

    struct RtParseResult {
        std::optional<ip::address> addr;
        std::optional<NetworkInterface> iface;
    };

public:
    InterfaceMonitorImpl(asio::io_context & ctxt,  const refcnt_ptr<Config> & config):
        m_config(config),
        m_socket(ctxt, raw_protocol(AF_NETLINK, NETLINK_ROUTE)),
        m_recvBuffer(2 * m_config->pageSize()) {

        sockaddr_nl addr = {};
        addr.nl_family = AF_NETLINK;
        addr.nl_groups = RTMGRP_LINK;
        if (m_config->enableIPv4())
            addr.nl_groups |= RTMGRP_IPV4_IFADDR;
        if (m_config->enableIPv6()) 
            addr.nl_groups |= RTMGRP_IPV6_IFADDR;
        m_socket.bind(raw_protocol::endpoint(&addr, sizeof(addr)));
    }

    void start(Handler & handler) override {
        m_handler = &handler;
        WSDLOG_INFO("Starting interface monitor");
        requestAll();
        read();
    }

    void stop() override {
        WSDLOG_INFO("Stopping interface monitor");
        m_socket.close();
        m_handler = nullptr;
    }
private:
    void requestAll() {
        struct {
            nlmsghdr header;
            rtgenmsg msg;
        } message;
        message.header.nlmsg_len = sizeof(message);
        message.header.nlmsg_type = RTM_GETADDR;
        message.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
        message.header.nlmsg_seq = 1;
        message.header.nlmsg_pid = 0;
        message.msg.rtgen_family = AF_PACKET;

        sockaddr_nl addr = {};
        addr.nl_family = AF_NETLINK;

        m_socket.send_to(asio::buffer(&message, sizeof(message)), raw_protocol::endpoint(&addr, sizeof(addr)));
    }

    void read() {
        asio::mutable_buffer buffer(m_recvBuffer.data(), m_recvBuffer.size());
        m_socket.async_receive(buffer, 
            [this, hodler = refcnt_retain(this)](const asio::error_code & ec, size_t bytesRead){

            if (!m_handler)
                return;
            
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    WSDLOG_CRITICAL("error reading from netlink socket {}", ec.message());
                    m_handler->onFatalInterfaceMonitorError(ec);
                }
                
                return;
            }

            auto res = parseBuffer(m_recvBuffer.data(), m_recvBuffer.data() + bytesRead);
            switch (res) {
            case Done:
            case ExpectMore:
                break;
            case BufferTooSmall:
                WSDLOG_WARN("interface monitor buffer is insufficient to hold a single message, increasing");
                m_recvBuffer.resize(m_recvBuffer.size() * 2);
                //fallthrough
            case Error:
                requestAll();
            }

            read();
        });
    }

    auto parseBuffer(const std::byte * first, const std::byte * last) ->  ParseStatus {

        std::unordered_map<int, bool> knownIfaces; 

        for(size_t len = 0; size_t(last - first) != 0; first += len) {

            if (size_t(last - first) < sizeof(nlmsghdr::nlmsg_len))
                return BufferTooSmall;

            auto cur = first;
            auto header = (const nlmsghdr *)cur;
            len = NLMSG_ALIGN(header->nlmsg_len);
            if (len > size_t(last - first)) 
                return BufferTooSmall;

            if (len < NLMSG_HDRLEN) {
                WSDLOG_WARN("nlmsghdr reported size {0} is less than size of nlmsghdr", len);
                return Error;
            }

            switch (header->nlmsg_type) {
            case NLMSG_ERROR:
                return Error;
            case NLMSG_OVERRUN:
                return BufferTooSmall;
            case NLMSG_DONE:
                return Done;
            case RTM_NEWADDR:
            case RTM_DELADDR:
                break;
            default:
                continue;
            }

            cur += NLMSG_HDRLEN;

            constexpr size_t ifaddrmsgSize = NLMSG_ALIGN(sizeof(ifaddrmsg));
            if (size_t(last - cur) < ifaddrmsgSize)
                continue;

            auto msg = (const ifaddrmsg *)cur;
            if (msg->ifa_flags & (IFA_F_DADFAILED | IFA_F_HOMEADDRESS | IFA_F_DEPRECATED | IFA_F_TENTATIVE))
                continue;
            if (msg->ifa_family != AF_INET && msg->ifa_family != AF_INET6)
                continue;

            
            cur += ifaddrmsgSize;
            size_t remaining = last - cur;

            auto [addr, iface] = parseRtAttr(msg->ifa_index, msg->ifa_family, cur, remaining);
            
            if (!iface || !addr)
                continue;

            if (!m_config->isAllowedInterface(iface->name)) {
                WSDLOG_DEBUG("Interface {} is not allowed in configuration - ignoring", *iface);
                continue;
            }

            handleDetected(header->nlmsg_type == RTM_NEWADDR, *iface, *addr, knownIfaces);
        }
        return ExpectMore;
    }

    auto parseRtAttr(decltype(ifaddrmsg::ifa_index) ifIndex, 
                     decltype(ifaddrmsg::ifa_family) addrFamily,
                     const std::byte * ptr, size_t remaining) -> RtParseResult {

        RtParseResult res;
        for (auto * rta = (const rtattr *)ptr; RTA_OK(rta, remaining); rta = RTA_NEXT(rta, remaining)) {
             auto * data = RTA_DATA(rta);

            switch(rta->rta_type) {
            case IFA_LABEL: {
                auto len = strnlen((const char *)data, RTA_PAYLOAD(rta));
                res.iface.emplace(ifIndex, (const char *)data, (const char *)data + len);
                break;
            }
            case IFA_LOCAL:
                if (addrFamily == AF_INET && m_config->enableIPv4()) {
                    if (RTA_PAYLOAD(rta) >= sizeof(ip::address_v4::bytes_type)) {
                        res.addr.emplace(ip::address_v4(*(ip::address_v4::bytes_type*)data));
                    }
                }
                break;
            case IFA_ADDRESS:
                if (addrFamily == AF_INET6 && m_config->enableIPv6()) {
                    if (RTA_PAYLOAD(rta) >= sizeof(ip::address_v6::bytes_type)) {
                        ip::address_v6 addr6(*(ip::address_v6::bytes_type*)data);
                        if (addr6.is_link_local()) {
                            addr6.scope_id(ifIndex);
                            res.addr.emplace(addr6);
                        }
                    }
                }
                break;
            }
        }

        if (!res.iface && res.addr) {
            if (auto nameRes = ioctlSocket<GetInterfaceName>(m_socket, ifIndex))
                res.iface.emplace(ifIndex, nameRes.assume_value());
            else
                WSDLOG_ERROR("Unable to obtain name for interface {0}, {1}", ifIndex, nameRes.assume_error().message());
        } 

        return res;
    }

    void handleDetected(bool isAdded, NetworkInterface iface, ip::address addr, std::unordered_map<int, bool> & knownIfaces) {
        
        if (isAdded) {
            bool ignore = true;
            if (auto it = knownIfaces.find(iface.index); it == knownIfaces.end()) {
                if (auto flagsRes = ioctlSocket<GetInterfaceFlags>(m_socket, iface.name)) {
                    auto flags = flagsRes.assume_value();
                    ignore = !(interfaceFlags & IFF_MULTICAST);
                    if (!ignore && !m_config->enableLoopback()) {
                        ignore = (interfaceFlags & IFF_LOOPBACK);
                    }
                    knownIfaces[iface.index] = ignore;
                } else {
                    WSDLOG_ERROR("Unable to obtain flags for interface {0}, {1}", iface, flagsRes.assume_error().message());
                }
            } else {
                ignore = it->second;
            }
                
            if (!ignore)
                m_handler->addAddress(iface, addr);
            else if (m_config->enableLoopback())
                WSDLOG_DEBUG("Interface {} doesn't support multicast - ignoring", iface);
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
};

auto createInterfaceMonitor(asio::io_context & ctxt, const refcnt_ptr<Config> & config) -> refcnt_ptr<InterfaceMonitor> {
    return refcnt_attach(new InterfaceMonitorImpl(ctxt, config));
}

#endif
