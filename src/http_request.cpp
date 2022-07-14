// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "http_request.h"

using namespace sysstr;

auto HttpRequest::getUniqueHeader(const sys_string & name) const -> Outcome<std::optional<sys_string>> {
    auto [first, last] = this->headers.equal_range(name);
    if (first == last)
        return std::nullopt;
    if (--last != first)
        return HeaderError::NotUnique;
    return first->second;
}

auto HttpRequest::getHeaderList(const sys_string & name) const -> std::optional<sys_string> {
    auto [first, last] = this->headers.equal_range(name);
    if (first == last)
        return std::nullopt;
    sys_string_builder builder;
    builder.append(first->second);
    for (++first; first == last; ++first) {
        builder.append(S(", "));
        builder.append(first->second);
    }
    return builder.build();
}

auto HttpRequest::getContentLength() const -> Outcome<std::optional<size_t>> {
    auto maybeVal = getUniqueHeader(S("Content-Length"));
    if (!maybeVal)
        return maybeVal.assume_error();
    
    auto val = maybeVal.assume_value();
    if (!val)
        return std::nullopt;

    size_t ret;
    auto first = val->c_str();
    auto last = first + val->storage_size();
    auto res = std::from_chars(first, last, ret);
    if (res.ec != std::errc() || res.ptr != last) {
        return HeaderError::BadFormat;
    }
    return ret;
}

auto HttpRequest::getContentType() const -> Outcome<std::optional<std::vector<sys_string>>> {
    auto maybeVal = getUniqueHeader(S("Content-Type"));
    if (!maybeVal)
        return maybeVal.assume_error();
    
    auto val = maybeVal.assume_value();

    if (!val)
        return std::nullopt;
    std::vector<sys_string> ret;
    val->split(std::back_inserter(ret), S("; "));
    return ret;
}

auto HttpRequest::getKeepAlive() const -> bool {
    auto val = getHeaderList(S("Connection"));
    if (!val)
        return false;
    std::set<sys_string> items;
    val->split(std::inserter(items, items.end()), S(", "));
    return items.contains(S("keep-alive"));
}