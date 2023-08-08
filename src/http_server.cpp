// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "http_server.h"
#include "http_request_parser.h"
#include "http_response.h"
#include "xml_wrapper.h"
#include "util.h"
#include "exc_handling.h"

using namespace isptr;
using namespace sysstr;

namespace ip = asio::ip;

class HttpServerImpl;

class HttpConnection : public ref_counted<HttpConnection> {
    friend ref_counted<HttpConnection>;

private:
    enum class State {
        InHeader,
        InBody
    };
    enum class ParseResult {
        Continue,
        Error,
        Done
    };
public:
    HttpConnection(const refcnt_ptr<Config> & config, ip::tcp::socket && socket):
        m_config(config),
        m_socket(std::move(socket)),
        m_callerDesc(m_socket.remote_endpoint().address().to_string()) {
    }

    void start(HttpServerImpl & owner) {
        m_owner = &owner;
        read();
        WSDLOG_DEBUG("HTTP from {}, starting", m_callerDesc);
    }

    void stop() {
        WSDLOG_DEBUG("HTTP from {}, stopping", m_callerDesc);
        m_socket.close();
        m_owner = nullptr;
    }
private:
    ~HttpConnection() noexcept {
    }

    void read();
    void write(bool final);

    auto parseIncoming(const std::byte * first, const std::byte * last) -> ParseResult;
    auto parseHeader(const std::byte * first, const std::byte * last) -> std::pair<ParseResult, const std::byte *>;
    auto parseBody(const std::byte * first, const std::byte * last) -> std::pair<ParseResult, const std::byte *>;

private:
    refcnt_ptr<Config> m_config;
    ip::tcp::socket m_socket;
    sys_string m_callerDesc;
    
    HttpServerImpl * m_owner = nullptr;
    bool m_stopRequested = false;
    std::array<std::byte, 8192> m_readBuffer;

    State m_state = State::InHeader;
    HttpRequestParser m_headerParser;
    HttpRequest m_request;
    HttpResponse m_response;
    size_t m_contentRemaining = 0;
    bool m_keepAlive = false;
    std::unique_ptr<XmlParserContext> m_contentParser;
};

class HttpServerImpl : public HttpServer {
public:
    HttpServerImpl(asio::io_context & ctxt, const refcnt_ptr<Config> & config,
                   const NetworkInterface & iface, const ip::tcp::endpoint & endpoint);

    void start(Handler & handler) override {
        WSDLOG_INFO("Starting HTTP server on {}", m_iface);
        m_handler = &handler;
        accept();
    }
    
    void stop() override {
        WSDLOG_INFO("Stopping HTTP server on {}", m_iface);
        m_handler = nullptr;
        m_acceptor.close();
        for(auto & con: m_connections) {
            con->stop();
        }
        m_connections.clear();
    }

    auto handleHttpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer>;
    void onConnectionFinished(const refcnt_ptr<HttpConnection> & con);
private:
    void accept();

private:
    ~HttpServerImpl() noexcept {
    }

    void handleConnection(ip::tcp::socket && socket);
private:
    refcnt_ptr<Config> m_config;
    NetworkInterface m_iface;
    Handler * m_handler = nullptr;
    ip::tcp::acceptor m_acceptor;

    std::set<refcnt_ptr<HttpConnection>> m_connections;
};

auto createHttpServer(asio::io_context & ctxt, 
                      const refcnt_ptr<Config> & config,
                      const NetworkInterface & iface,
                      const ip::tcp::endpoint & endpoint) -> refcnt_ptr<HttpServer> {
    return make_refcnt<HttpServerImpl>(ctxt, config, iface, endpoint);
}

HttpServerImpl::HttpServerImpl(asio::io_context & ctxt, const refcnt_ptr<Config> & config,
                               const NetworkInterface & iface, const ip::tcp::endpoint & endpoint):
    m_config(config),
    m_iface(iface),
    m_acceptor(ctxt) {

    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(ip::tcp::socket::reuse_address(true));
    if (endpoint.address().is_v6()) {
        m_acceptor.set_option(ip::v6_only(true));
    }
    m_acceptor.bind(endpoint);
}

void HttpServerImpl::accept() {
    m_acceptor.listen();
    m_acceptor.async_accept(
        [this, holder = refcnt_retain(this)](asio::error_code ec, ip::tcp::socket socket) {

        if (!m_handler)
            return;
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                WSDLOG_ERROR("HTTP server on {}, error accepting: {}", m_iface, ec.message());
                m_handler->onFatalHttpError();
            }
            
            return;
        }
        
        handleConnection(std::move(socket));
        accept();
    });
}

void HttpServerImpl::handleConnection(ip::tcp::socket && socket) {
    auto connection = make_refcnt<HttpConnection>(m_config, std::move(socket));
    m_connections.insert(connection);
    connection->start(*this);
}

void HttpServerImpl::onConnectionFinished(const refcnt_ptr<HttpConnection> & connection) {
    m_connections.erase(connection);
    connection->stop();
}

auto HttpServerImpl::handleHttpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer> {
    
    if (m_handler)
        return m_handler->handleHttpRequest(std::move(doc));
    return std::nullopt;
}

void HttpConnection::read() {
    m_socket.async_read_some(asio::buffer(m_readBuffer), 
        [this, holder = refcnt_retain(this)] (asio::error_code ec, size_t bytesRead) {

        if (!m_owner)
            return;
        
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                WSDLOG_DEBUG("HTTP from {}, error reading: {}", m_callerDesc, ec.message());
                m_owner->onConnectionFinished(holder);
            }
            
            return;
        }

        auto parseRes = parseIncoming(m_readBuffer.data(), m_readBuffer.data() + bytesRead);
        switch(parseRes) {
            break;case ParseResult::Continue: read();
            break;case ParseResult::Done:     write(!m_keepAlive);
            break;case ParseResult::Error:    write(true);
        }
    });
}

void HttpConnection::write(bool finalWrite)
{
    asio::async_write(m_socket, m_response.makeBuffers(),
        [this, holder = refcnt_retain(this), finalWrite](asio::error_code ec, size_t) {
        
        if (!m_owner)
            return;
        
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                WSDLOG_DEBUG("HTTP from {}, error writing: {}", m_callerDesc, ec.message());
                m_owner->onConnectionFinished(holder);
            }
            
            return;
        }

        if (!finalWrite) {
            read();
            return;
        } 

        asio::error_code ignore;
        m_socket.shutdown(ip::tcp::socket::shutdown_both, ignore);
        m_owner->onConnectionFinished(holder);
    });
}

auto HttpConnection::parseIncoming(const std::byte * first, const std::byte * last) -> ParseResult {

    while(first != last) {
        std::pair<ParseResult, const std::byte *> res;
        switch(m_state) {
            break; case State::InHeader: res = parseHeader(first, last);
            break; case State::InBody: res = parseBody(first, last);
        }
        if (res.first != ParseResult::Continue)
            return res.first;
        first = res.second;
    }
    return ParseResult::Continue;
}

auto HttpConnection::parseHeader(const std::byte * first, const std::byte * last) -> std::pair<ParseResult, const std::byte *> {
    auto [res, readEnd] = m_headerParser.parse(m_request, first, last);

    if (res == HttpRequestParser::Bad) {
        WSDLOG_INFO("HTTP from {}: bad HTTP request", m_callerDesc);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    
    if (res == HttpRequestParser::Indeterminate) {
        assert(readEnd == last);
        return {ParseResult::Continue, last};
    }

    WSDLOG_DEBUG("HTTP from {}: {} {}", m_callerDesc, m_request.method, m_request.uri);
    
    if (m_request.method != S("POST") || m_request.uri != S("/") + m_config->httpPath()) {
        m_response = HttpResponse::makeStockResponse(HttpResponse::NotFound);
        return {ParseResult::Error, readEnd};
    }

    auto contentLengthRes = m_request.getContentLength();
    if (!contentLengthRes || !contentLengthRes.assume_value()) {
        WSDLOG_INFO("HTTP from {}: missing Content-Length header", m_callerDesc);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    m_contentRemaining = *contentLengthRes.assume_value();
    

    auto contentTypeRes = m_request.getContentType();
    if (!contentTypeRes) {
        WSDLOG_INFO("HTTP from {}: missing Content-Type header", m_callerDesc);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    if (contentTypeRes.assume_value()) {
        const std::vector<sys_string> & contentTypeParts = *contentTypeRes.assume_value();
        if (contentTypeParts.size() < 1 || contentTypeParts.size() > 2 || contentTypeParts[0] != S("application/soap+xml")) {
            WSDLOG_INFO("HTTP from {}: invalid Content-Type '{}'", m_callerDesc,
                         S(",").join(contentTypeParts.begin(), contentTypeParts.end()));
            m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
            return {ParseResult::Error, readEnd};
        }
        if (contentTypeParts.size() == 2) {
            if (!contentTypeParts[1].starts_with(S("charset="))) {
                WSDLOG_INFO("HTTP from {}: invalid Content-Type '{}'", m_callerDesc,
                             S(",").join(contentTypeParts.begin(), contentTypeParts.end()));
                m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
                return {ParseResult::Error, readEnd};
            }
            auto charset = contentTypeParts[1].remove_prefix(S("charset="));
            m_contentParser = XmlParserContext::createPush(xml_str(charset));
        } else {
            m_contentParser = XmlParserContext::createPush();
        }
    } else {
        m_contentParser = XmlParserContext::createPush();
    }

    m_keepAlive = m_request.getKeepAlive();

    m_headerParser.reset();
    m_state = State::InBody;
    return {ParseResult::Continue, readEnd};
}

auto HttpConnection::parseBody(const std::byte * first, const std::byte * last) -> std::pair<ParseResult, const std::byte *> {

    size_t chunkSize = std::min(m_contentRemaining, size_t(last - first));
    m_contentRemaining -= chunkSize;
    
    WSDLOG_TRACE("HTTP from {}, received {}", m_callerDesc, std::string_view((const char *)first, chunkSize));
    
    try {
        //int cast is safe because our buffer is much much smaller than max int (8192 currently)
        m_contentParser->parseChunk((const uint8_t *)first, int(chunkSize), m_contentRemaining == 0);
    } catch(std::exception & ex) {
        WSDLOG_INFO("HTTP from {}: error parsing XML {}", m_callerDesc, ex.what());
        WSDLOG_TRACE("{}", formatCaughtExceptionBacktrace());
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, first + chunkSize};
    }
    
    if (m_contentRemaining == 0) {
        if (!m_contentParser->wellFormed()) {
            WSDLOG_INFO("HTTP from {}: XML is not well formed", m_callerDesc);
            m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
            return {ParseResult::Error, first + chunkSize};
        }

        auto doc = m_contentParser->extractDoc();
        std::optional<XmlCharBuffer> maybeReply;
        try {
            maybeReply = m_owner->handleHttpRequest(std::move(doc));
        } catch(std::exception & ex) {
            WSDLOG_ERROR("HTTP from {}: error handling request: {}", m_callerDesc, ex.what());
            WSDLOG_TRACE("{}", formatCaughtExceptionBacktrace());
        }

        if (!maybeReply) {
            m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
            return {ParseResult::Error, first + chunkSize};
        }

        WSDLOG_TRACE("HTTP from {}, sending: {}", m_callerDesc,
                      std::string_view((const char *)maybeReply->data(), maybeReply->size()));
        m_response = HttpResponse::makeReply(std::move(*maybeReply));
        m_state = State::InHeader;
        m_contentParser.reset();

        return { ParseResult::Done, first + chunkSize};
    }

    return {ParseResult::Continue, last};
}

