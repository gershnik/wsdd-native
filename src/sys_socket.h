// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SYS_SOCKET_H_INCLUDED
#define HEADER_SYS_SOCKET_H_INCLUDED

#include "sys_util.h"


template<class Protocol, class Executor, class T>
void setSocketOption(asio::basic_socket<Protocol, Executor> & socket, const T & option) {
    int res = setsockopt(socket.native_handle(), T::level, T::name, option.addr(), option.size());
    if (res != 0)
        throwErrno("setsockopt()", errno);
}

template<int Level, int Name, class T, bool IsPrimitive = !std::is_class_v<T> && !std::is_union_v<T>>
struct SockOpt;

template<int Level, int Name, class T>
struct SockOpt<Level, Name, T, /*IsPrimitive*/true> {
    static constexpr int level = Level;
    static constexpr int name  = Name;
    T value;

    const void * addr() const {
        return &value;
    }
    socklen_t size() const {
        return socklen_t(sizeof(T));
    }
};

template<int Level, int Name, class T>
struct SockOpt<Level, Name, T, /*IsPrimitive*/false> : T {
    static constexpr int level = Level;
    static constexpr int name  = Name;

    const void * addr() const {
        return static_cast<const T *>(this);
    }
    socklen_t size() const {
        return socklen_t(sizeof(T));
    }
};

template<int Level, int Name, class T>
struct SockOptRef {
    static constexpr int level = Level;
    static constexpr int name  = Name;
    T * value;

    const void * addr() const {
        return value;
    }
    socklen_t size() const {
        return socklen_t(sizeof(T));
    }
};


using SockOptIpAddMembershipRef = SockOptRef<IPPROTO_IP, IP_ADD_MEMBERSHIP, ip_mreqn>;
using SockOptIpMulticastInterfaceRef = SockOptRef<IPPROTO_IP, IP_MULTICAST_IF, ip_mreqn>;
using SockOptIpv6MulticastInterface = SockOpt<IPPROTO_IPV6, IPV6_MULTICAST_IF, int>;

#ifdef __linux__
    using SockOptIpMulticastAll = SockOpt<IPPROTO_IP, IP_MULTICAST_ALL, int>;
    using SockOptIpv6MulticastAll = SockOpt<IPPROTO_IPV6, IPV6_MULTICAST_ALL, int>;
#endif

template<unsigned long Name, class T>
class SocketIOControl {
public:
    constexpr auto name() const -> unsigned long { return Name; }
    auto data() -> void * { return &m_data; }
    
protected:
    T m_data;
};

class GetInterfaceFlags : public SocketIOControl<SIOCGIFFLAGS, ifreq> {

public:
    GetInterfaceFlags(const sys_string & name) {
        auto copied = name.copy_data(0, m_data.ifr_name, IFNAMSIZ);
        memset(m_data.ifr_name + copied, 0, IFNAMSIZ - copied);
    }
    
    auto result() const -> std::remove_cvref_t<decltype(m_data.ifr_flags)> {
        return m_data.ifr_flags;
    }
};


#ifdef __linux__

    class GetInterfaceName : public SocketIOControl<SIOCGIFNAME, ifreq> {
    public:
        GetInterfaceName(int ifIndex) {
            m_data.ifr_ifindex  = ifIndex;
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
