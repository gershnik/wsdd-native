// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SYS_SOCKET_H_INCLUDED
#define HEADER_SYS_SOCKET_H_INCLUDED

#include "sys_util.h"

namespace ptl {

    template<class Protocol, class Executor> 
    struct FileDescriptorTraits<asio::basic_datagram_socket<Protocol, Executor>> {
        [[gnu::always_inline]] static int c_fd(asio::basic_datagram_socket<Protocol, Executor> & socket) noexcept
            { return socket.native_handle();}
    };

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
