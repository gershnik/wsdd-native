
// Copyright (c) 2022, Eugene Gershnik
// Copyright (c) 2003-2022 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// SPDX-License-Identifier: BSD-3-Clause


#ifndef HEADER_HTTP_REQUEST_PARSER_H_INCLUDED
#define HEADER_HTTP_REQUEST_PARSER_H_INCLUDED

#include "http_request.h"

/**
 Parser for HTTP 1.x request header
 */
class HttpRequestParser
{
private:
    static constexpr std::tuple<unsigned, unsigned> s_minVersion{1, 0};
    static constexpr std::tuple<unsigned, unsigned> s_maxVersion{1, 1};
    static constexpr size_t s_maxMethodSize = 10;
    static constexpr size_t s_maxUriSize = 2048;
    static constexpr size_t s_maxHeadersSize = 8192;
public:
    /// Reset to initial parser state.
    void reset();

    /// Result of parse.
    enum ResultType { Good, Bad, Indeterminate };

    /// Parse some data. The enum return value is Good when a complete request has
    /// been parsed, Bad if the data is invalid, Indeterminate when more data is
    /// required. The InputIterator return value indicates how much of the input
    /// has been consumed.
    template <typename InputIterator>
    requires(sizeof(typename std::iterator_traits<InputIterator>::value_type) == 1)
    auto parse(HttpRequest & req, InputIterator begin, InputIterator end) ->
        std::tuple<ResultType, InputIterator>
    {
        while (begin != end)
        {
            ResultType result = consume(req, uint8_t(*begin++));
            if (result == Good || result == Bad)
                return {result, begin};
        }
        return {Indeterminate, begin};
    }

private:
    /// Handle the next character of input.
    auto consume(HttpRequest & req, uint8_t input) -> ResultType;

    /// Check if a byte is an HTTP character.
    static bool isChar(uint8_t c) {
        return c <= 127;
    }

    /// Check if a byte is an HTTP control character.
    static bool isCtl(uint8_t c) {
        return c <= 31 || c == 127;
    }

    /// Check if a byte is defined as an HTTP tspecial character.
    static bool isTSpecial(uint8_t c) {
        switch (c)
        {
        case u8'(': case u8')': case u8'<': case u8'>':  case u8'@':
        case u8',': case u8';': case u8':': case u8'\\': case u8'"':
        case u8'/': case u8'[': case u8']': case u8'?':  case u8'=':
        case u8'{': case u8'}': case u8' ': case u8'\t':
            return true;
        default:
            return false;
        }
    }

    /// Check if a byte is a digit.
    static bool isDigit(char8_t c) {
        return c >= u8'0' && c <= u8'9';
    }

    /// The current state of the parser.
    enum State
    {
        method_start,
        method,
        uri,
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3
    } m_state = method_start;

    sys_string_builder m_methodBuilder;
    sys_string_builder m_uriBuilder;
    unsigned m_versionMajor = 0;
    unsigned m_versionMinor = 0;
    sys_string_builder m_headerNameBuilder;
    sys_string_builder m_headerValueBuilder;
    unsigned m_totalHeadersSize = 0;
};


#endif 
