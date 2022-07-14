// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_HTTP_SERVER_H_INCLUDED
#define HEADER_HTTP_SERVER_H_INCLUDED

#include "config.h"
#include "xml_wrapper.h"

struct NetworkInterface;

class HttpServer : public ref_counted<HttpServer> {
    friend ref_counted<HttpServer>;

public:
    class Handler {
    public:
        virtual auto handleHttpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer> = 0;
        virtual void onFatalHttpError() = 0;
    protected:
        ~Handler() {}
    };
public:
    virtual void start(Handler & handler) = 0;
    virtual void stop() = 0;
protected:
    HttpServer() {
    }
    virtual ~HttpServer() noexcept {
    }
};

using HttpServerFactoryT = auto (asio::io_context & ctxt,
                                 const refcnt_ptr<Config> & config,
                                 const NetworkInterface & iface,
                                 const ip::tcp::endpoint & endpoint) -> refcnt_ptr<HttpServer>;
using HttpServerFactory = std::function<HttpServerFactoryT>;

HttpServerFactoryT createHttpServer;

#endif 
