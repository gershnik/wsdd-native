// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "config.h"
#include "util.h"

using namespace std::literals;

static auto startsWithAndHasMore(const std::vector<char> & buf, std::string_view str) -> bool {
    if (buf.size() < str.size() + 1)
        return false;
    if (memcmp(buf.data(), str.data(), str.size()) != 0)
        return false;

    return true;
}

static auto tryToRunSambaToGetConfig(const char * path) -> std::optional<std::filesystem::path> {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        WSDLOG_TRACE("path '{}' is not a regular file", path);
        return std::nullopt;
    }

    std::string command(path);
    command += " --show-build";

    StdIoPipe pipe(command.c_str(), "r", ec);
    if (!pipe) {
        WSDLOG_TRACE("'{}' cannot be run, error {}", command, strerror(errno));
        return std::nullopt;
    }

    auto prefix = "   CONFIGFILE: "sv;
    std::vector<char> buf;
    buf.reserve(1024);
    for (bool eof = false; !eof; ) {
        buf.clear();
        bool truncated = false;
        auto res = readLine(pipe, [&](char c){
            if (buf.size() < 1024) {
                buf.push_back(c);
            } else {
                WSDLOG_WARN("'{}' produced an overly long line", command);
                truncated = false;
            }
        });
        if (!res) {
            WSDLOG_WARN("failed to read output of '{}', error {}", command, res.assume_error().message());
            break;
        }
        eof = !res.assume_value();
        if (truncated)
            continue;
        if (!startsWithAndHasMore(buf, prefix))
            continue;
        return std::filesystem::path(buf.data() + prefix.size(), buf.data() + buf.size());
    }
    return std::nullopt;
}

auto Config::findSmbConf() -> std::optional<std::filesystem::path> {
    
    auto prefix = "samba: "sv;

    WSDLOG_DEBUG("trying to locate samba");
    std::error_code ec;
    const char command[] = "whereis -b samba";
    StdIoPipe pipe(command, "r", ec);
    if (!pipe) {
        WSDLOG_ERROR("'{}' cannot be run, error {}", command, strerror(errno));
        return std::nullopt;
    }
    std::vector<char> buf;
    buf.reserve(1024);
    auto res = readLine(pipe, [&](char c) {
        if (buf.size() < 1024) {
            buf.push_back(c);
        } else {
            WSDLOG_WARN("'{}' output is overly long, truncated", command);
        }
    });
    if (!res) {
        WSDLOG_ERROR("failed to read output of '{}', error {}", command, res.assume_error().message());
        return std::nullopt;
    }
    if (!startsWithAndHasMore(buf, prefix)) {
        WSDLOG_ERROR("output of '{}' does not match '{}<more chars>'", command, prefix);
        WSDLOG_DEBUG("output: {}", std::string_view(buf.data(), buf.size()));
        return std::nullopt;
    }
    
    buf.push_back(' '); //ensure we have clean termination
    char * pathStart = buf.data() + prefix.size();
    char * bufEnd = buf.data() + buf.size();
    char * pathEnd = pathStart;
    for (; pathEnd != bufEnd; ++pathEnd) {
        if (*pathEnd == ' ') {
            *pathEnd = '\0';
            WSDLOG_DEBUG("trying '{}' as samba", pathStart);
            auto res = tryToRunSambaToGetConfig(pathStart);
            if (res) {
                WSDLOG_INFO("found samba config at '{}'", res->c_str());
                return res;
            }
            pathStart = pathEnd + 1;
        }
    }
    return std::nullopt;
}

 auto Config::readSmbConf(const std::filesystem::path & path, bool useNetbiosHostName) -> std::optional<WinNetInfo> {

    std::error_code ec;
    auto file = StdIoFile(path.c_str(), "r", ec);
    if (!file)
        return std::nullopt;

    std::regex sectionRe(R"##(\s*\[([^\]]*)\].*)##", std::regex_constants::ECMAScript);
    std::regex entryRe(R"##(\s*([^ \t#;=][^=#;]*)\s*=\s*((?:[^ \t#;][^#;]*)?).*)##", std::regex_constants::ECMAScript);

    std::optional<sys_string> workgroup;
    std::optional<sys_string> hostName;
    std::optional<sys_string> hostDescription;
    bool isDomain = false;
    bool inGlobalSection = false;

    std::vector<char> buf;
    buf.reserve(1024);
    for (bool eof = false; !eof; ) {
        buf.clear();
        bool truncated = false;
        auto res = readLine(file, [&](char c) {
            if (buf.size() < 1024)
                buf.push_back(c);
            else
                truncated = true;
        });
        if (!res)
            break;
        eof = !res.assume_value();
        
        if (truncated)
            continue;

        buf.push_back('\0');

        std::cmatch m;

        if (std::regex_match(buf.data(), m, sectionRe)) {
            if (inGlobalSection) {
                WSDLOG_TRACE("smb.conf global section has ended");
                break;
            }
            inGlobalSection = (std::string_view(m[1].first, m[1].length()) == "global"sv);
            WSDLOG_TRACE("found smb.conf global section");
            continue;
        }

        if (!inGlobalSection)
            continue;

        if (!std::regex_match(buf.data(), m, entryRe))
            continue;
        std::string_view key(m[1].first, m[1].length());
        while(!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.remove_suffix(1);
        std::string_view value(m[2].first, m[2].length());
        while(!value.empty() && (value.back() == ' ' || value.back() == '\t'))
            value.remove_suffix(1);

        if (key == "workgroup"sv) {
            WSDLOG_TRACE("smb.conf workgroup is: {}", value);
            workgroup.emplace(value);
        } else if (key == "security"sv) {
            WSDLOG_TRACE("smb.conf security is: {}", value);
            isDomain = (value == "domain"sv || value == "ads"sv);
        } else if (key == "netbios name"sv) {
            WSDLOG_TRACE("smb.conf netbios name is: {}", value);
            hostName.emplace(value);
        } else if (key == "server string"sv) {
            WSDLOG_TRACE("smb.conf server string is: {}", value);
            hostDescription.emplace(value);
        }
    }
    WSDLOG_TRACE("smb.conf reading done");

    WinNetInfo ret;

    if (workgroup && !workgroup->empty()) {
        if (isDomain)
            ret.memberOf.emplace<WindowsDomain>(*workgroup);
        else
            ret.memberOf.emplace<WindowsWorkgroup>(*workgroup);
    } else {
        WSDLOG_WARN("smb.conf indicates membership of a domain but contains no domain name, assuming workgroup");
        ret.memberOf.emplace<WindowsWorkgroup>(S("WORKGROUP"));
    }

    if (useNetbiosHostName) {
        if (hostName && !hostName->empty())
            ret.hostName = *hostName;
        else
            ret.hostName = m_simpleHostName.to_upper();
    } else {
        ret.hostName = m_simpleHostName;
    }

    if (hostDescription && !hostDescription->empty()) {
        auto desc = *hostDescription;
        desc = desc.replace(S("%h"), m_simpleHostName);
        if (!desc.contains(U'%')) {
            ret.hostDescription = desc;
        } else {
            WSDLOG_WARN("smb.conf server string contains unsppported % tokens, using host name as description");
            ret.hostDescription = m_simpleHostName;
        }
    } else {
        ret.hostDescription = m_simpleHostName;
    }

    
    return ret;
}
