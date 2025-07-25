// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "config.h"
#include "util.h"
#include "sys_util.h"

using namespace std::literals;

static auto startsWithAndHasMore(const std::string_view & buf, std::string_view str) -> bool {
    if (buf.size() < str.size() + 1)
        return false;
    if (memcmp(buf.data(), str.data(), str.size()) != 0)
        return false;

    return true;
}


static auto findSmbConfViaSamba() -> std::optional<std::filesystem::path> {

    WSDLOG_DEBUG("trying to locate samba");

    std::filesystem::path conf;

    try {
        auto prefix = "   CONFIGFILE: "sv;
        shell({"samba", "--show-build"}, false, LineReader(1024, [&](std::string_view line) {
            if (startsWithAndHasMore(line, prefix)) {
                conf = std::filesystem::path(line.substr(prefix.size()));
            }
        }));

        WSDLOG_INFO("found samba config at '{}'", conf.c_str());
        return conf;
    } catch(std::exception & ex) {
        WSDLOG_DEBUG("`samba --show-build` failed: {}", ex.what());
    }

    return {};
}

#ifdef __linux__
static auto findKsmbConf() -> std::optional<std::filesystem::path> {
    std::array paths = {
        "/etc/ksmbd/ksmbd.conf",
        "/usr/local/etc/ksmbd/ksmbd.conf"
    };
    for (auto * p: paths) {
        WSDLOG_DEBUG("checking '{}' existence", p);
        std::filesystem::path testPath{p};
        std::error_code ec;
        if (exists(testPath, ec)) {
            WSDLOG_INFO("found samba config at '{}'", p);
            return testPath;
        }
    }
    return {};
}
#endif

static auto findSmbConf() -> std::optional<std::filesystem::path> {

    if (auto res = findSmbConfViaSamba())
        return res;
    #ifdef __linux__
        return findKsmbConf();
    #else
        return {};
    #endif
}

static auto paramsFromSmbConf(const std::filesystem::path & path) -> std::optional<Config::SambaParams> {

    std::error_code ec;
    auto file = ptl::FileDescriptor::open(path.c_str(), O_RDONLY, ec);
    if (!file)
        return {};

    struct ::stat st;
    getStatus(file, st, ec);
    if (ec)
        return {};

    void * bytes = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, c_fd(file), 0);
    if (bytes == MAP_FAILED)
        return {};
    

    WSDLOG_TRACE("reading smb.conf");

    std::string_view content((const char *)bytes, st.st_size);

    std::regex sectionRe(R"##(\s*\[([^\]]*)\].*)##", std::regex_constants::ECMAScript);
    std::regex entryRe(R"##(\s*([^ \t#;=][^=#;]*)\s*=\s*((?:[^ \t#;][^#;]*)?).*)##", std::regex_constants::ECMAScript);

    Config::SambaParams ret;
    bool inGlobalSection = false;
    
    auto processed_end = content.begin();
    for(auto cur = processed_end, end = content.end(); cur != end; ) {

        if (*cur != '\n') {
            ++cur;
            continue;
        }

        std::string_view line(processed_end, cur);
        processed_end = ++cur;

        std::match_results<std::string_view::const_iterator> m;

        if (std::regex_match(line.begin(), line.end(), m, sectionRe)) {
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

        if (!std::regex_match(line.begin(), line.end(), m, entryRe))
            continue;
        std::string_view key(m[1].first, m[1].length());
        while(!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.remove_suffix(1);
        std::string_view value(m[2].first, m[2].length());
        while(!value.empty() && (value.back() == ' ' || value.back() == '\t'))
            value.remove_suffix(1);

        if (key == "workgroup"sv) {
            WSDLOG_TRACE("smb.conf workgroup is: {}", value);
            ret.workgroup.emplace(value);
        } else if (key == "security"sv) {
            WSDLOG_TRACE("smb.conf security is: {}", value);
            ret.security.emplace(value);
        } else if (key == "netbios name"sv) {
            WSDLOG_TRACE("smb.conf netbios name is: {}", value);
            ret.hostName.emplace(value);
        } else if (key == "server string"sv) {
            WSDLOG_TRACE("smb.conf server string is: {}", value);
            ret.hostDescription.emplace(value);
        }
    }

    WSDLOG_TRACE("smb.conf reading done");
    return ret;
}

static auto runTestParm(std::string_view name) -> std::optional<sys_string> {

    std::string arg = "--parameter-name=";
    arg += name;

    std::optional<sys_string> ret;

    shell({"testparm", "-sl", arg.c_str()}, true, LineReader(1024, [&](std::string_view line) {
        ret = line;
    }));

    return ret;
}

auto paramsFromTestParm() -> std::optional<Config::SambaParams> {

    WSDLOG_DEBUG("trying to use testparm to detect samba config");

    try {
        Config::SambaParams ret;
        ret.workgroup = runTestParm("workgroup"sv);
        WSDLOG_TRACE("testparm workgroup is: {}", ret.workgroup.value_or(S("")));
        ret.security = runTestParm("security"sv);
        WSDLOG_TRACE("testparm security is: {}", ret.security.value_or(S("")));
        ret.hostName = runTestParm("netbios name"sv);
        WSDLOG_TRACE("testparm netbios name is: {}", ret.hostName.value_or(S("")));
        ret.hostDescription = runTestParm("server string"sv);
        WSDLOG_TRACE("testparm server string is: {}", ret.hostDescription.value_or(S("")));

        WSDLOG_INFO("found samba config via testparm tool");
        return ret;

    } catch(std::exception & ex) {
        WSDLOG_DEBUG("testparm detection failed: {}", ex.what());
    }

    return {};
}

auto Config::sambaParamsToWinNetInfo(const SambaParams & params, bool useNetbiosHostName) -> WinNetInfo {
    
    bool isDomain = (params.security == S("domain") || params.security == S("ads"));
    
    WinNetInfo ret;

    if (params.workgroup && !params.workgroup->empty()) {
        if (isDomain)
            ret.memberOf.emplace<WindowsDomain>(*params.workgroup);
        else
            ret.memberOf.emplace<WindowsWorkgroup>(*params.workgroup);
    } else {
        WSDLOG_WARN("Samba config indicates membership in a domain but contains no domain name, assuming workgroup");
        ret.memberOf.emplace<WindowsWorkgroup>(S("WORKGROUP"));
    }

    if (useNetbiosHostName) {
        if (params.hostName && !params.hostName->empty())
            ret.hostName = *params.hostName;
        else
            ret.hostName = m_simpleHostName.to_upper();
    } else {
        ret.hostName = m_simpleHostName;
    }

    if (params.hostDescription && !params.hostDescription->empty()) {
        auto desc = *params.hostDescription;
        desc = desc.replace(S("%h"), m_simpleHostName);
        if (!desc.contains(U'%')) {
            ret.hostDescription = desc;
        } else {
            WSDLOG_WARN("Samba config server string contains unsppported % tokens, using host name as description");
            ret.hostDescription = m_simpleHostName;
        }
    } else {
        ret.hostDescription = m_simpleHostName;
    }

    
    return ret;
}

auto Config::detectWinNetInfo(std::optional<std::filesystem::path> smbConf, bool useNetbiosHostName) -> std::optional<WinNetInfo> {

    if (!smbConf)
        smbConf = findSmbConf();
    
    std::optional<Config::SambaParams> params;
    if (smbConf)
        params = paramsFromSmbConf(*smbConf);
    if (!params)
        params = paramsFromTestParm();
    if (params)
        return sambaParamsToWinNetInfo(*params, useNetbiosHostName);

    return {};
}
