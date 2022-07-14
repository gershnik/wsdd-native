
// Copyright (c) 2022, Eugene Gershnik
// Copyright (c) 2003-2022 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// SPDX-License-Identifier: BSD-3-Clause

#include "http_request_parser.h"

static inline
auto boundedAddDigit(unsigned & value, unsigned digit, unsigned maxVal) -> bool {

    unsigned val = value;
    if (maxVal / 10  < val)
        return false;
    val *= 10;
    if (maxVal - digit < val)
        return false;
    val += digit;

    value = val;
    return true;
}

void HttpRequestParser::reset() {
    m_state = method_start;
    m_methodBuilder.clear();
    m_uriBuilder.clear();
    m_versionMajor = 0;
    m_versionMinor = 0;
    m_headerNameBuilder.clear();
    m_headerValueBuilder.clear();
    m_totalHeadersSize = 0;
}

auto HttpRequestParser::consume(HttpRequest & req, uint8_t input) -> ResultType {
    switch (m_state) {
    case method_start:
        if (!isChar(input) || isCtl(input) || isTSpecial(input))
            return Bad;
        
        m_methodBuilder.append(input);
        m_state = method;
        return Indeterminate;
    
    case method:
        if (input == u8' ') {
            req.method = m_methodBuilder.build();
            m_state = uri;
            return Indeterminate;
        }
        if (!isChar(input) || isCtl(input) || isTSpecial(input))
            return Bad;
    
        if (m_methodBuilder.storage_size() == s_maxMethodSize)
            return Bad;
        m_methodBuilder.append(input);
        return Indeterminate;
    
    case uri:
        if (input == u8' ') {
            if (m_uriBuilder.empty())
                return Bad;

            req.uri = m_uriBuilder.build();
            m_state = http_version_h;
            return Indeterminate;
        }
        if (isCtl(input))
            return Bad;
      
        if (m_uriBuilder.storage_size() == s_maxUriSize)
            return Bad;
        m_uriBuilder.append(input);
        return Indeterminate;
  
    case http_version_h:
        if (input == u8'H') {
            m_state = http_version_t_1;
            return Indeterminate;
        }
        return Bad;

    case http_version_t_1:
        if (input == u8'T') {
            m_state = http_version_t_2;
            return Indeterminate;
        }
        return Bad;
    
    case http_version_t_2:
        if (input == u8'T') {
            m_state = http_version_p;
            return Indeterminate;
        }
        return Bad;
    
    case http_version_p:
        if (input == u8'P') {
            m_state = http_version_slash;
            return Indeterminate;
        }
        return Bad;
    
    case http_version_slash:
        if (input == u8'/') {
            m_state = http_version_major_start;
            return Indeterminate;
        }
        return Bad;
    
    case http_version_major_start:
        if (isDigit(input)) {
            unsigned digit = input - '0';
            if (digit == 0 || digit > std::get<0>(s_maxVersion))
                return Bad;
            m_versionMajor = digit; 
            m_state = http_version_major;
            return Indeterminate;
        }
        return Bad;
  
    case http_version_major:
        if (input == u8'.') {
            if (m_versionMajor < std::get<0>(s_minVersion))
                return Bad;
            m_state = http_version_minor_start;
            return Indeterminate;
        }
        if (isDigit(input)) {
            unsigned digit = input - '0';
            unsigned maxMajor = std::get<0>(s_maxVersion);
            if (!boundedAddDigit(m_versionMajor, digit, maxMajor))
                return Bad;
            return Indeterminate;
        }
        return Bad;
    
    case http_version_minor_start:
        if (isDigit(input)) {
            unsigned digit = input - '0';
            if (m_versionMajor == std::get<0>(s_maxVersion) && digit > std::get<1>(s_maxVersion))
                return Bad;
            m_versionMinor = digit;
            m_state = http_version_minor;
            return Indeterminate;
        }
        return Bad;
    
    case http_version_minor:
        if (input == u8'\r') {
            if (m_versionMajor == std::get<0>(s_minVersion) && m_versionMinor < std::get<1>(s_minVersion))
                return Bad;
            req.versionMajor = m_versionMajor;
            req.versionMinor = m_versionMinor;
            m_state = expecting_newline_1;
            return Indeterminate;
        }
        if (isDigit(input)) {
            unsigned digit = input - '0';
            unsigned maxMinor = m_versionMajor == std::get<0>(s_maxVersion) ? std::get<1>(s_maxVersion) : std::numeric_limits<unsigned>::max();
            if (!boundedAddDigit(m_versionMinor, digit, maxMinor))
                return Bad;
            return Indeterminate;
        }
        return Bad;

    case expecting_newline_1:
        if (input == u8'\n') {
            m_state = header_line_start;
            return Indeterminate;
        }
        return Bad;
    
    case header_line_start:
        if (input == u8'\r') {
            m_state = expecting_newline_3;
            return Indeterminate;
        }
        if (!m_headerValueBuilder.empty() && (input == u8' ' || input == u8'\t')) {
            m_state = header_lws;
            return Indeterminate;
        }
        if (!isChar(input) || isCtl(input) || isTSpecial(input))
            return Bad;
    
        if (m_totalHeadersSize == s_maxHeadersSize)
            return Bad;
        m_headerNameBuilder.append(input);
        ++m_totalHeadersSize;
        m_state = header_name;
        return Indeterminate;
    
    case header_lws:
        if (input == u8'\r') {
            m_state = expecting_newline_2;
            return Indeterminate;
        }
        if (input == u8' ' || input == u8'\t') 
            return Indeterminate;
    
        if (isCtl(input))
            return Bad;
    
        m_state = header_value;
        if (m_totalHeadersSize == s_maxHeadersSize)
            return Bad;
        m_headerValueBuilder.append(input);
        ++m_totalHeadersSize;
        return Indeterminate;
    
    case header_name:
        if (input == u8':') {

            m_state = space_before_header_value;
            return Indeterminate;
        }
        if (!isChar(input) || isCtl(input) || isTSpecial(input))
            return Bad;

        if (m_totalHeadersSize == s_maxHeadersSize)
            return Bad;
        m_headerNameBuilder.append(input);
        ++m_totalHeadersSize;
        return Indeterminate;
    
    case space_before_header_value:
        if (input == u8' ') {
            m_state = header_value;
            return Indeterminate;
        }
        return Bad;
    
    case header_value:
        if (input == u8'\r') {
            m_state = expecting_newline_2;
            return Indeterminate;
        }
        if (isCtl(input))
            return Bad;
    
        if (m_totalHeadersSize == s_maxHeadersSize)
            return Bad;
        m_headerValueBuilder.append(input);
        ++m_totalHeadersSize;
        return Indeterminate;
    
    case expecting_newline_2:
        if (input == u8'\n') {
            req.headers.emplace(m_headerNameBuilder.build(), m_headerValueBuilder.build());
            m_state = header_line_start;
            return Indeterminate;
        }
        return Bad;
    
    case expecting_newline_3:
        return (input == u8'\n') ? Good : Bad;
  
    }

    return Bad;
}


