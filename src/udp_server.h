// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_UDP_SERVER_H_INCLUDED
#define HEADER_UDP_SERVER_H_INCLUDED

#include "xml_wrapper.h"
#include "util.h"
#include "config.h"

class UdpServer : public ref_counted<UdpServer> {
    friend ref_counted<UdpServer>;

public:
    class Handler {
    public:
        virtual auto handleUdpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer> = 0;
        virtual void onFatalUdpError() = 0;
    protected:
        ~Handler() {}
    };
public:
    virtual void start(Handler & handler) = 0;
    virtual void stop() = 0;
    virtual void broadcast(XmlCharBuffer && data, std::function<void (asio::error_code)> continuation = nullptr) = 0;

protected:
    UdpServer() {
    }
    virtual ~UdpServer() noexcept {
    }    
};

using UdpServerFactoryT = auto (asio::io_context & ctxt,
                                const refcnt_ptr<Config> & config,
                                const NetworkInterface & iface,
                                const ip::address & addr) -> refcnt_ptr<UdpServer>;
using UdpServerFactory = std::function<UdpServerFactoryT>;

UdpServerFactoryT createUdpServer;


#endif
