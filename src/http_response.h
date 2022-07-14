
// Copyright (c) 2022, Eugene Gershnik
// Copyright (c) 2003-2022 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// SPDX-License-Identifier: BSD-3-Clause


#ifndef HEADER_HTTP_RESPONSE_H_INCLUDED
#define HEADER_HTTP_RESPONSE_H_INCLUDED

#include "xml_wrapper.h"

class HttpResponse
{
public:
    enum Status
    {
        Ok = 200,
        Created = 201,
        Accepted = 202,
        NoContent = 204,
        MultipleChoices = 300,
        MovedPermanently = 301,
        MovedTemporarily = 302,
        NotModified = 304,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnavailable = 503
    };

public:
    HttpResponse(Status status = InternalServerError) : m_status(status) {
    }
    static auto makeStockResponse(Status status) -> HttpResponse;
    static auto makeReply(XmlCharBuffer && xml) -> HttpResponse;

    void addHeader(const sys_string & name, const sys_string & value);

    auto makeBuffers() const -> std::vector<asio::const_buffer>;

private:
    auto contentLength() const -> size_t;

private:
    Status m_status;
    std::variant<sys_string, XmlCharBuffer, std::u8string_view> m_content;
    std::vector<sys_string> m_headers;
};


#endif 
