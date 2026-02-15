// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SYS_SOCKET_H_INCLUDED
#define HEADER_SYS_SOCKET_H_INCLUDED

#include "sys_util.h"

#if __has_include(<sys/sockio.h>)
#include <sys/sockio.h>
#endif

#if __has_include(<sys/ioctl.h>)
#include <sys/ioctl.h>
#endif

namespace ptl {

    template<class Protocol, class Executor> 
    struct FileDescriptorTraits<asio::basic_datagram_socket<Protocol, Executor>> {
        [[gnu::always_inline]] static int c_fd(asio::basic_datagram_socket<Protocol, Executor> & socket) noexcept
            { return socket.native_handle();}
    };

}

#define    IN6_IS_SCOPE_LINKLOCAL(a)    \
    ((IN6_IS_ADDR_LINKLOCAL(a)) ||    \
    (IN6_IS_ADDR_MC_LINKLOCAL(a)))


WSDDN_DECLARE_MEMBER_DETECTOR(in6_addr, s6_addr16, in6_addr_has_s6_addr16);

template<std::same_as<in6_addr> T>
static inline uint16_t * in6_addr_addr16(T & addr) {
    if constexpr (in6_addr_has_s6_addr16)
        return addr.s6_addr16;
    else
        return (uint16_t *)&addr.s6_addr;
}


inline auto makeAddress(const sockaddr_in & addr) -> ip::address_v4 {
    return ip::address_v4(ntohl(addr.sin_addr.s_addr));
}

inline auto makeAddress(const sockaddr_in6 & addr) -> ip::address_v6 {
    union {
        ip::address_v6::bytes_type asio;
        in6_addr raw;
    } clearAddr;
    memcpy(&clearAddr.raw, addr.sin6_addr.s6_addr, sizeof(clearAddr.raw));
    uint32_t scope = addr.sin6_scope_id;
    if (IN6_IS_SCOPE_LINKLOCAL(&clearAddr.raw)) {
        uint16_t * words = in6_addr_addr16(clearAddr.raw);
        if (uint32_t embeddedScope = htons(words[1])) {
            scope = embeddedScope;
        }
        words[1] = 0;
    }
    return ip::address_v6(clearAddr.asio, scope);
}

WSDDN_DECLARE_MEMBER_DETECTOR(struct ifreq, ifr_ifindex, ifreq_has_ifr_ifindex);

template<std::same_as<struct ifreq> T>
static inline void set_ifreq_ifindex(T & req, int ifIndex) {
    if constexpr (ifreq_has_ifr_ifindex)
        req.ifr_ifindex = ifIndex;
    else
        req.ifr_index = ifIndex;
}

template<std::same_as<struct ifreq> T>
static inline int ifreq_ifindex(const T & req) {
    if constexpr (ifreq_has_ifr_ifindex)
        return req.ifr_ifindex;
    else
        return req.ifr_index;
}

template<unsigned long Name, class T>
class SocketIOControl {
public:
    constexpr auto name() const -> unsigned long { return Name; }
    auto data() -> void * { return &m_data; }
    
protected:
    T m_data;
};

#ifdef SIOCGIFFLAGS

    class GetInterfaceFlags : public SocketIOControl<(unsigned long)SIOCGIFFLAGS, ifreq> {

    public:
        GetInterfaceFlags(const sys_string & name) {
            auto copied = name.copy_data(0, m_data.ifr_name, IFNAMSIZ);
            memset(m_data.ifr_name + copied, 0, IFNAMSIZ - copied);
        }
        
        auto result() const -> std::remove_cvref_t<decltype(this->m_data.ifr_flags)> {
            return m_data.ifr_flags;
        }
    };
    
#endif

#ifdef SIOCGLIFCONF

    class GetLInterfaceConf : public SocketIOControl<(unsigned long)SIOCGLIFCONF, lifconf> {
    public:
        GetLInterfaceConf(sa_family_t family, lifreq * dest, size_t size) {
            m_data.lifc_family = family;
            m_data.lifc_flags = 0;
            m_data.lifc_len = size * sizeof(lifreq);
            m_data.lifc_req = dest;
        }

        auto result() const -> size_t {
            return m_data.lifc_len / sizeof(lifreq);
        }
    };

#endif

#ifdef SIOCGIFCONF

    class GetInterfaceConf : public SocketIOControl<(unsigned long)SIOCGIFCONF, ifconf> {
    public:
        GetInterfaceConf(ifreq * dest, size_t size) {
            using lentype = decltype(m_data.ifc_len);
            m_data.ifc_len = lentype(size * sizeof(ifreq));
            m_data.ifc_req = dest;
        }

        auto result() const -> size_t {
            return m_data.ifc_len / sizeof(ifreq);
        }
    };

#endif

#ifdef SIOCGLIFINDEX

    class GetLInterfaceIndex : public SocketIOControl<(unsigned long)SIOCGLIFINDEX, lifreq> {
    public:
        GetLInterfaceIndex(const sys_string & name) {
            auto copied = name.copy_data(0, m_data.lifr_name, IFNAMSIZ);
            memset(m_data.lifr_name + copied, 0, IFNAMSIZ - copied);
        }

        auto result() const -> int {
            return m_data.lifr_index;
        }
    };

#endif

#ifdef SIOCGIFINDEX

    class GetInterfaceIndex : public SocketIOControl<(unsigned long)SIOCGIFINDEX, ifreq> {
    public:
        GetInterfaceIndex(const sys_string & name) {
            auto copied = name.copy_data(0, m_data.ifr_name, IFNAMSIZ);
            memset(m_data.ifr_name + copied, 0, IFNAMSIZ - copied);
        }

        auto result() const -> int {
            return ifreq_ifindex(m_data);
        }
    };

#endif

#ifdef SIOCGLIFFLAGS

    class GetLInterfaceFlags : public SocketIOControl<(unsigned long)SIOCGLIFFLAGS, lifreq> {

    public:
        GetLInterfaceFlags(const sys_string & name) {
            auto copied = name.copy_data(0, m_data.lifr_name, IFNAMSIZ);
            memset(m_data.lifr_name + copied, 0, IFNAMSIZ - copied);
        }
        
        auto result() const -> std::remove_cvref_t<decltype(this->m_data.lifr_flags)> {
            return m_data.lifr_flags;
        }
    };

#endif


#ifdef SIOCGIFNAME

    class GetInterfaceName : public SocketIOControl<(unsigned long)SIOCGIFNAME, ifreq> {
    public:
        GetInterfaceName(int ifIndex) {
            set_ifreq_ifindex(m_data, ifIndex);
        }
        
        auto result() const -> sys_string {
            auto len = strnlen(m_data.ifr_name, IFNAMSIZ);
            return sys_string(m_data.ifr_name, len);
        }

    };

#endif

template<class IoControl, class Protocol, class Executor, class... Args>
auto ioctlSocket(asio::basic_socket<Protocol, Executor> & socket, Args && ...args) ->
    outcome::result<decltype(std::declval<IoControl>().result())> {
        
    IoControl control(std::forward<Args>(args)...);

    asio::error_code ec;
    socket.io_control(control, ec);
    if (ec)
        return ec;
    return control.result();
}

#endif
