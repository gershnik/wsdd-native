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

static constexpr size_t g_httpMaxConnectionsFromSameAddress = 20;
static constexpr size_t g_httpMaxContentLength = 256 * 1024;
static constexpr auto g_httpMaxConnectionDuration = std::chrono::seconds(5);

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
        m_remoteAddr(m_socket.remote_endpoint().address()),
        m_startTime(std::chrono::steady_clock::now())
    {}

    void start(HttpServerImpl & owner);
    void stop();

    auto remoteAddress() const -> const ip::address & {
        return m_remoteAddr;
    }
    auto startTime() const -> const std::chrono::steady_clock::time_point & {
        return m_startTime;
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
    ip::address m_remoteAddr;
    std::chrono::steady_clock::time_point m_startTime;
    sys_string m_connDesc;
    
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
        WSDLOG_INFO("{}: starting server", m_serverDesc);
        m_handler = &handler;
        accept();
    }
    
    void stop() override {
        WSDLOG_INFO("{}: stopping server", m_serverDesc);
        m_handler = nullptr;
        m_acceptor.close();
        m_gcTimer.cancel();
        for(auto & con: m_connections) {
            con->stop();
        }
        m_connections.clear();
    }

    auto handleHttpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer>;
    void onConnectionFinished(const refcnt_ptr<HttpConnection> & con);

    auto serverDesc() const -> const sys_string & 
        { return m_serverDesc; }
private:
    void accept();
    void scheduleGC();

private:
    ~HttpServerImpl() noexcept {
    }

    void handleConnection(ip::tcp::socket && socket);
private:
    refcnt_ptr<Config> m_config;
    Handler * m_handler = nullptr;
    ip::tcp::acceptor m_acceptor;
    asio::steady_timer m_gcTimer;
    sys_string m_serverDesc;

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
    m_acceptor(ctxt),
    m_gcTimer(ctxt),
    m_serverDesc(sys_format("HTTP on {}({})", iface.name, endpoint.address().is_v6() ? "v6" : "v4")) {

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
                WSDLOG_ERROR("{}: error accepting: {}", m_serverDesc, ec.message());
                m_handler->onFatalHttpError();
            }
            
            return;
        }
        
        handleConnection(std::move(socket));
        accept();
    });
}

void HttpServerImpl::handleConnection(ip::tcp::socket && socket) {

    bool wasEmpty = m_connections.empty();
    
    auto remoteAddr = socket.remote_endpoint().address();
    size_t sameAddrCount = 0;
    refcnt_ptr<HttpConnection> oldestWithTheSameAddr;
    for(auto & con: m_connections) {
        if (con->remoteAddress() == remoteAddr) {
            if (!oldestWithTheSameAddr || oldestWithTheSameAddr->startTime() < con->startTime())
                oldestWithTheSameAddr = con;
            ++sameAddrCount;
        }
    }
    if (sameAddrCount >= g_httpMaxConnectionsFromSameAddress) {
        WSDLOG_INFO("{}: too many simultaneous connections from {}, dropping oldest", m_serverDesc, remoteAddr.to_string());
        oldestWithTheSameAddr->stop();
        m_connections.erase(oldestWithTheSameAddr);
    }

    auto connection = make_refcnt<HttpConnection>(m_config, std::move(socket));
    m_connections.insert(connection);
    connection->start(*this);
    if (wasEmpty)
        scheduleGC();
}

void HttpServerImpl::scheduleGC() {
    m_gcTimer.expires_after(g_httpMaxConnectionDuration);
    m_gcTimer.async_wait([this, holder = refcnt_retain(this)](asio::error_code ec) {

        if (!m_handler)
            return;

        if (ec) {
            if (ec != asio::error::operation_aborted) {
                WSDLOG_ERROR("{}: error waiting for gc timer: {}", m_serverDesc, ec.message());
                m_handler->onFatalHttpError();
            }
            
            return;
        }

        auto now = std::chrono::steady_clock::now();
        for (auto it = m_connections.begin(), last = m_connections.end(); it != last; ) {
            auto & con = *it;
            if (now - con->startTime() > g_httpMaxConnectionDuration) {
                WSDLOG_INFO("{}: dropping stale connection from {}", m_serverDesc, con->remoteAddress().to_string());
                con->stop();
                it = m_connections.erase(it);
            } else {
                ++it;
            }
        }

        if (!m_connections.empty())
            scheduleGC();
    });
}

void HttpServerImpl::onConnectionFinished(const refcnt_ptr<HttpConnection> & connection) {
    m_connections.erase(connection);
    connection->stop();
    if (m_connections.empty())
        m_gcTimer.cancel();
}

auto HttpServerImpl::handleHttpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer> {
    
    if (m_handler)
        return m_handler->handleHttpRequest(std::move(doc));
    return std::nullopt;
}

void HttpConnection::start(HttpServerImpl & owner) {
    m_owner = &owner;
    m_connDesc = sys_format("{}, from {}", owner.serverDesc(), m_remoteAddr.to_string());
    read();
    WSDLOG_DEBUG("{}: connection start", m_connDesc);
}

void HttpConnection::stop() {
    WSDLOG_DEBUG("{}: connection end", m_connDesc);
    m_socket.close();
    m_owner = nullptr;
}

void HttpConnection::read() {
    m_socket.async_read_some(asio::buffer(m_readBuffer), 
        [this, holder = refcnt_retain(this)] (asio::error_code ec, size_t bytesRead) {

        if (!m_owner)
            return;
        
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                WSDLOG_DEBUG("{}: error reading: {}", m_connDesc, ec.message());
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
                WSDLOG_DEBUG("{}: error writing: {}", m_connDesc, ec.message());
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
        WSDLOG_INFO("{}: bad HTTP request", m_connDesc);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    
    if (res == HttpRequestParser::Indeterminate) {
        assert(readEnd == last);
        return {ParseResult::Continue, last};
    }

    WSDLOG_DEBUG("{}: {} {}", m_connDesc, m_request.method, m_request.uri);
    
    if (m_request.method != S("POST") || m_request.uri != S("/") + m_config->httpPath()) {
        m_response = HttpResponse::makeStockResponse(HttpResponse::NotFound);
        return {ParseResult::Error, readEnd};
    }

    auto contentLengthRes = m_request.getContentLength();
    if (!contentLengthRes || !contentLengthRes.assume_value()) {
        WSDLOG_INFO("{}: missing Content-Length header", m_connDesc);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    auto contentLength = *contentLengthRes.assume_value();
    if (contentLength > g_httpMaxContentLength) {
        WSDLOG_INFO("{}: Content-Length {} is too big", m_connDesc, contentLength);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    m_contentRemaining = contentLength;
    

    auto contentTypeRes = m_request.getContentType();
    if (!contentTypeRes) {
        WSDLOG_INFO("{}: missing Content-Type header", m_connDesc);
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, readEnd};
    }
    if (contentTypeRes.assume_value()) {
        const std::vector<sys_string> & contentTypeParts = *contentTypeRes.assume_value();
        if (contentTypeParts.size() < 1 || contentTypeParts.size() > 2 || contentTypeParts[0] != S("application/soap+xml")) {
            WSDLOG_INFO("{}: invalid Content-Type '{}'", m_connDesc,
                         S(",").join(contentTypeParts.begin(), contentTypeParts.end()));
            m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
            return {ParseResult::Error, readEnd};
        }
        if (contentTypeParts.size() == 2) {
            if (!contentTypeParts[1].starts_with(S("charset="))) {
                WSDLOG_INFO("{}: invalid Content-Type '{}'", m_connDesc,
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

#if LIBXML_VERSION >= 21300
    m_contentParser->useOptions(XML_PARSE_NO_XXE, XML_PARSE_NO_XXE);
#endif

    m_keepAlive = m_request.getKeepAlive();

    m_headerParser.reset();
    m_state = State::InBody;
    return {ParseResult::Continue, readEnd};
}

auto HttpConnection::parseBody(const std::byte * first, const std::byte * last) -> std::pair<ParseResult, const std::byte *> {

    size_t chunkSize = std::min(m_contentRemaining, size_t(last - first));
    m_contentRemaining -= chunkSize;
    
    WSDLOG_TRACE("{}: received {}", m_connDesc, std::string_view((const char *)first, chunkSize));
    
    try {
        //int cast is safe because our buffer is much much smaller than max int (8192 currently)
        m_contentParser->parseChunk((const uint8_t *)first, int(chunkSize), m_contentRemaining == 0);
    } catch(std::exception & ex) {
        WSDLOG_INFO("{}: error parsing XML {}", m_connDesc, ex.what());
        WSDLOG_TRACE("{}", formatCaughtExceptionBacktrace());
        m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
        return {ParseResult::Error, first + chunkSize};
    }
    
    if (m_contentRemaining == 0) {
        if (!m_contentParser->wellFormed()) {
            WSDLOG_INFO("{}: XML is not well formed", m_connDesc);
            m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
            return {ParseResult::Error, first + chunkSize};
        }

        auto doc = m_contentParser->extractDoc();
        std::optional<XmlCharBuffer> maybeReply;
        try {
            maybeReply = m_owner->handleHttpRequest(std::move(doc));
        } catch(std::exception & ex) {
            WSDLOG_ERROR("{}: error handling request: {}", m_connDesc, ex.what());
            WSDLOG_TRACE("{}", formatCaughtExceptionBacktrace());
        }

        if (!maybeReply) {
            m_response = HttpResponse::makeStockResponse(HttpResponse::BadRequest);
            return {ParseResult::Error, first + chunkSize};
        }

        WSDLOG_TRACE("{}: sending: {}", m_connDesc,
                      std::string_view((const char *)maybeReply->data(), maybeReply->size()));
        m_response = HttpResponse::makeReply(std::move(*maybeReply));
        m_state = State::InHeader;
        m_contentParser.reset();

        return { ParseResult::Done, first + chunkSize};
    }

    return {ParseResult::Continue, last};
}

