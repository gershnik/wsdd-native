// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_HTTP_REQUEST_H_INCLUDED
#define HEADER_HTTP_REQUEST_H_INCLUDED

struct HttpRequest {

    enum class HeaderError {
        NotUnique = 1,
        BadFormat
    };

    template<class T>
    using Outcome = outcome::outcome<T, HeaderError>;

    auto getUniqueHeader(const sys_string & name) const -> Outcome<std::optional<sys_string>>;
    auto getHeaderList(const sys_string & name) const -> std::optional<sys_string>;

    auto getContentLength() const -> Outcome<std::optional<size_t>>;
    auto getContentType() const -> Outcome<std::optional<std::vector<sys_string>>>;
    auto getKeepAlive() const -> bool;

    sys_string method;
    sys_string uri;
    unsigned versionMajor;
    unsigned versionMinor;
    std::multimap<sys_string, sys_string> headers;
};


#endif 

