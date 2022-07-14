// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "server_manager.h"
#include "exc_handling.h"

auto ServerManager::createServer(const NetworkInterface & interface, const ip::address & addr) -> refcnt_ptr<WsdServer> {
    refcnt_ptr<WsdServer> server;
    try {
        server = createWsdServer(m_ctxt, m_config, m_httpServerFactory, m_udpServerFactory, interface, addr);
    } catch(std::system_error & ex) {
        WSDLOG_ERROR("Unable to start WSD server on interface {}, addr {}: error: {}", interface, addr.to_string(), ex.what());
        WSDLOG_DEBUG("{}", formatCaughtExceptionBacktrace());
        return nullptr;
    }
    server->start();
    return server;
}

void ServerManager::onFatalInterfaceMonitorError(asio::error_code ec) {
    //nothing left to do if the monitor died
    throw std::system_error(ec);
}

void ServerManager::addAddress(const NetworkInterface & interface, const ip::address & addr) {
    auto & server = m_serversByAddress[addr];
    
    if (server && server->interface() == interface && server->state() == WsdServer::Running)
        return;
    
    WSDLOG_INFO("Adding interface {}, addr {}", interface, addr.to_string());
    server = createServer(interface, addr);
}

void ServerManager::removeAddress(const NetworkInterface & interface, const ip::address & addr) {
    
    auto itServer = m_serversByAddress.find(addr);
    if (itServer == m_serversByAddress.end())
        return;
    
    auto & server = itServer->second;
    if (server && server->interface() != interface)
        return;
    
    WSDLOG_INFO("Removing interface {}, addr {}", interface, addr.to_string());
    if (server)
        server->stop(false);
    m_serversByAddress.erase(itServer);
        
}
