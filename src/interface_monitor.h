// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_INTERFACE_MONITOR_H_INCLUDED
#define HEADER_INTERFACE_MONITOR_H_INCLUDED

#include "wsd_server.h"

class InterfaceMonitor : public ref_counted<InterfaceMonitor> {
    friend ref_counted<InterfaceMonitor>;
public:
    class Handler {
    public:
        virtual void addAddress(const NetworkInterface & interface, const ip::address & addr) = 0;
        virtual void removeAddress(const NetworkInterface & interface, const ip::address & addr) = 0;
        virtual void onFatalInterfaceMonitorError(asio::error_code ec) = 0;
    protected:
        ~Handler() {}
    };

public:
    virtual void start(Handler & handler) = 0;
    virtual void stop() = 0;
protected:
    InterfaceMonitor() {
    }
    virtual ~InterfaceMonitor() noexcept {
    }
};

using InterfaceMonitorFactoryT = auto (asio::io_context & ctxt, const refcnt_ptr<Config> & config) -> refcnt_ptr<InterfaceMonitor>;
using InterfaceMonitorFactory = std::function<InterfaceMonitorFactoryT>;

InterfaceMonitorFactoryT createInterfaceMonitor;

#endif 
