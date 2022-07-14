// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_WSD_SERVER_H_INCLUDED
#define HEADER_WSD_SERVER_H_INCLUDED

#include "udp_server.h"
#include "http_server.h"


class WsdServer : public ref_counted<WsdServer> {
    friend ref_counted<WsdServer>;
public:
    enum State {
        NotStarted,
        Running,
        Stopped
    };
public:
    virtual void start() = 0;
    virtual void stop(bool graceful) = 0;
    
    auto state() const -> State {
        return m_state;
    }
    
    auto interface() const -> const NetworkInterface & {
        return m_interface;
    }
protected:
    WsdServer(const NetworkInterface & iface): m_interface(iface) {
    }
    virtual ~WsdServer() noexcept {
    }
    
protected:
    const NetworkInterface m_interface;
    
    State m_state = NotStarted;
};

using WsdServerFactoryT = auto (asio::io_context & ctxt,
                                const refcnt_ptr<Config> & config,
                                HttpServerFactory httpFactory,
                                UdpServerFactory udpFactory,
                                const NetworkInterface & iface,
                                const ip::address & addr) -> refcnt_ptr<WsdServer>;
using WsdServerFactory = std::function<WsdServerFactoryT>;

WsdServerFactoryT createWsdServer;


#endif 
