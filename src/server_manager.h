// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SERVER_MANAGER_H_INCLUDED
#define HEADER_SERVER_MANAGER_H_INCLUDED

#include "wsd_server.h"
#include "interface_monitor.h"


class ServerManager : public InterfaceMonitor::Handler {

public:
    ServerManager(asio::io_context & ctxt, 
                  const refcnt_ptr<Config> & config,
                  InterfaceMonitorFactory ifaceMonitorFactory,
                  HttpServerFactory httpServerFactory,
                  UdpServerFactory udpServerFactory) :
        m_ctxt(ctxt),
        m_config(config),
        m_interfaceMonitor(ifaceMonitorFactory(ctxt, config)),
        m_httpServerFactory(httpServerFactory),
        m_udpServerFactory(udpServerFactory) {

    }

    void start() {
        m_interfaceMonitor->start(*this);
    }
    
    void stop(bool gracefully) {
        m_interfaceMonitor->stop();
        for(auto & [_, server]: m_serversByAddress) {
            if (server)
                server->stop(gracefully);
        }
        m_serversByAddress.clear();
    }

private:
    void addAddress(const NetworkInterface & interface, const ip::address & addr) override;
    void removeAddress(const NetworkInterface & interface, const ip::address & addr) override;
    void onFatalInterfaceMonitorError(asio::error_code ec) override;
    
    auto createServer(const NetworkInterface & interface, const ip::address & addr) -> refcnt_ptr<WsdServer>;

private:
    asio::io_context & m_ctxt;
    const refcnt_ptr<Config> m_config;
    refcnt_ptr<InterfaceMonitor> m_interfaceMonitor;
    HttpServerFactory m_httpServerFactory;
    UdpServerFactory m_udpServerFactory;
    std::map<ip::address, refcnt_ptr<WsdServer>> m_serversByAddress;
};

#endif
