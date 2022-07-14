
// Copyright (c) 2022, Eugene Gershnik
// Copyright (c) 2003-2022 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// SPDX-License-Identifier: BSD-3-Clause


#include "http_response.h"

using namespace sysstr;

using StatusRecord = std::tuple<int, const char8_t *, const char8_t *>;

static const StatusRecord g_statuses[] = {
    {200, u8"HTTP/1.0 200 OK\r\n", u8""},
    {201, u8"HTTP/1.0 201 Created\r\n",
            u8"<html>"
            "<head><title>Created</title></head>"
            "<body><h1>201 Created</h1></body>"
            "</html>"},
    {202, u8"HTTP/1.0 202 Accepted\r\n",
            u8"<html>"
            "<head><title>Accepted</title></head>"
            "<body><h1>202 Accepted</h1></body>"
            "</html>"},
    {204, u8"HTTP/1.0 204 No Content\r\n",
            u8"<html>"
            "<head><title>No Content</title></head>"
            "<body><h1>204 Content</h1></body>"
            "</html>"},
    {300, u8"HTTP/1.0 300 Multiple Choices\r\n",
            u8"<html>"
            "<head><title>Multiple Choices</title></head>"
            "<body><h1>300 Multiple Choices</h1></body>"
            "</html>"},
    {301, u8"HTTP/1.0 301 Moved Permanently\r\n",
            u8"<html>"
            "<head><title>Moved Permanently</title></head>"
            "<body><h1>301 Moved Permanently</h1></body>"
            "</html>"},
    {302, u8"HTTP/1.0 302 Moved Temporarily\r\n",
            u8"<html>"
            "<head><title>Moved Temporarily</title></head>"
            "<body><h1>302 Moved Temporarily</h1></body>"
            "</html>"},
    {304, u8"HTTP/1.0 304 Not Modified\r\n",
            u8"<html>"
            "<head><title>Not Modified</title></head>"
            "<body><h1>304 Not Modified</h1></body>"
            "</html>"},
    {400, u8"HTTP/1.0 400 Bad Request\r\n",
            u8"<html>"
            "<head><title>Bad Request</title></head>"
            "<body><h1>400 Bad Request</h1></body>"
            "</html>"},
    {401, u8"HTTP/1.0 401 Unauthorized\r\n",
            u8"<html>"
            "<head><title>Unauthorized</title></head>"
            "<body><h1>401 Unauthorized</h1></body>"
            "</html>"},
    {403, u8"HTTP/1.0 403 Forbidden\r\n",
            u8"<html>"
            "<head><title>Forbidden</title></head>"
            "<body><h1>403 Forbidden</h1></body>"
            "</html>"},
    {404, u8"HTTP/1.0 404 Not Found\r\n",
            u8"<html>"
            "<head><title>Not Found</title></head>"
            "<body><h1>404 Not Found</h1></body>"
            "</html>"},
    {500, u8"HTTP/1.0 500 Internal Server Error\r\n",
            u8"<html>"
            "<head><title>Internal Server Error</title></head>"
            "<body><h1>500 Internal Server Error</h1></body>"
            "</html>"},
    {501, u8"HTTP/1.0 501 Not Implemented\r\n",
            u8"<html>"
            "<head><title>Not Implemented</title></head>"
            "<body><h1>501 Not Implemented</h1></body>"
            "</html>"},
    {502, u8"HTTP/1.0 502 Bad Gateway\r\n",
            u8"<html>"
            "<head><title>Bad Gateway</title></head>"
            "<body><h1>502 Bad Gateway</h1></body>"
            "</html>"},
    {503, u8"HTTP/1.0 503 Service Unavailable\r\n",
            u8"<html>"
            "<head><title>Service Unavailable</title></head>"
            "<body><h1>503 Service Unavailable</h1></body>"
            "</html>"}
};

static const char8_t g_crlf[] = { u8'\r', u8'\n' };

static auto findStatusRecord(HttpResponse::Status status) -> const StatusRecord * {
    auto ptr = std::lower_bound(std::begin(g_statuses), std::end(g_statuses), status, 
        [] (const StatusRecord & val, HttpResponse::Status st) {

        return std::get<0>(val) < st;
    });
    if (ptr != std::end(g_statuses) && std::get<0>(*ptr) == status)
        return ptr;
    return nullptr;
}

static const StatusRecord & g_defaultStatusRecord = [](){
    auto ret = findStatusRecord(HttpResponse::InternalServerError);
    assert(ret);
    return *ret;
}();


static asio::const_buffer makeBuffer(HttpResponse::Status status) {
    auto ptr = findStatusRecord(status);
    if (!ptr)
        ptr = &g_defaultStatusRecord;
    return asio::buffer(std::get<1>(*ptr), std::char_traits<char8_t>::length(std::get<1>(*ptr)));
}

auto HttpResponse::makeStockResponse(Status status) -> HttpResponse {
    HttpResponse ret(status);
    auto ptr = findStatusRecord(status);
    if (!ptr)
        ptr = &g_defaultStatusRecord;
    auto content = std::u8string_view(std::get<2>(*ptr), std::char_traits<char8_t>::length(std::get<2>(*ptr)));
    ret.m_content = content;
    
    ret.m_headers.reserve(2);
    ret.addHeader(S("Content-Type"), S("text/html"));
    ret.addHeader(S("Content-Length"), std::to_string(content.size()));
    return ret;
}

auto HttpResponse::makeReply(XmlCharBuffer && xml) -> HttpResponse {
    HttpResponse ret(Ok);
    
    ret.m_headers.reserve(2);
    ret.addHeader(S("Content-Type"), S("application/soap+xml"));
    ret.addHeader(S("Content-Length"), std::to_string(xml.size()));
    ret.m_content = std::move(xml);
    return ret;
}

void HttpResponse::addHeader(const sys_string & name, const sys_string & value) {

    sys_string_builder builder;
    builder.append(name);
    builder.append(u8": ");
    builder.append(value);
    builder.append(g_crlf, std::size(g_crlf));
    m_headers.emplace_back(builder.build());
}


auto HttpResponse::makeBuffers() const -> std::vector<asio::const_buffer>
{
    std::vector<asio::const_buffer> buffers;
    buffers.push_back(makeBuffer(m_status));
    for (auto & header: m_headers) {
        buffers.emplace_back(asio::buffer(header.data(), header.storage_size()));
    }
    buffers.push_back(asio::buffer(g_crlf));
    std::visit([&buffers](const auto & val) {
        using ContentType = std::remove_cvref_t<decltype(val)>;
        if constexpr (std::is_same_v<ContentType, sys_string>) {

            buffers.push_back(asio::buffer(val.data(), val.storage_size()));

        } else  {

            buffers.push_back(asio::buffer(val.data(), val.size()));
        }

    }, m_content);
    return buffers;
}

auto HttpResponse::contentLength() const -> size_t {
    return std::visit([](const auto & val) {
        using ContentType = std::remove_cvref_t<decltype(val)>;
        if constexpr (std::is_same_v<ContentType, sys_string>) {

            return val.storage_size();

        } else  {

            return val.size();
        }

    }, m_content);
}


