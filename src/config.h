// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_CONFIG_H_INCLUDED
#define HEADER_CONFIG_H_INCLUDED

#include "sys_util.h"
#include "util.h"
#include "xml_wrapper.h"

constexpr uint16_t g_WsdUdpPort = 3702;
constexpr uint16_t g_WsdHttpPort = 5357;

constexpr const char * g_WsdMulticastGroupV4 = "239.255.255.250";
constexpr const char * g_WsdMulticastGroupV6 = "ff02::c";  // link-local

constexpr size_t g_maxLogFileSize = 1024 * 1024;
constexpr size_t g_maxRotatedLogs = 5;

struct CommandLine;

class Config : public ref_counted<Config> {
    friend ref_counted<Config>;
public:
    struct WinNetInfo {
        sys_string hostName;
        sys_string hostDescription;
        MemberOf memberOf;
    };
public:
    static refcnt_ptr<Config> make(const CommandLine & cmdline) {
        return refcnt_attach(new Config(cmdline));
    }

    auto instanceIdentifier() const -> size_t               { return m_instanceIdentifier; };
    auto endpointIdentifier() const -> const sys_string &   { return m_urnUuid; }
    auto httpPath() const -> const sys_string &             { return m_strUuid; }
    auto winNetInfo() const -> const WinNetInfo &           { return m_winNetInfo; }
    auto metadataDoc() const -> XmlDoc *                    { return m_metadataDoc.get(); }
    
    auto enableIPv4() const -> bool                         { return m_allowedAddressFamily != IPv6Only; }
    auto enableIPv6() const -> bool                         { return m_allowedAddressFamily != IPv4Only; }
    auto hopLimit() const -> int                            { return m_hopLimit; }
    auto isAllowedInterface(const sys_string & name) const -> bool {
        return m_interfaceWhitelist.empty() || m_interfaceWhitelist.contains(name);
    }
    
    auto pageSize() const -> size_t                         { return m_pageSize; }

private:
    Config(const CommandLine & cmdline);
    ~Config() {};

#if HAVE_APPLE_SAMBA
    auto detectWinNetInfo(bool useNetbiosHostName) -> std::optional<WinNetInfo>;
#endif
    auto findSmbConf() -> std::optional<std::filesystem::path>;
    auto readSmbConf(const std::filesystem::path & path, bool useNetbiosHostName) -> std::optional<WinNetInfo>;
    
    auto getHostName() const -> sys_string;
    
    auto loadMetadaFile(const std::string & filename) const -> std::unique_ptr<XmlDoc>;
private:
    size_t m_instanceIdentifier;
    sys_string m_fullHostName;
    sys_string m_simpleHostName;
    Uuid m_uuid;
    sys_string m_strUuid;
    sys_string m_urnUuid;
    WinNetInfo m_winNetInfo;
    std::unique_ptr<XmlDoc> m_metadataDoc;
    
    AllowedAddressFamily m_allowedAddressFamily = BothIPv4AndIPv6;
    int m_hopLimit = 1;
    std::set<sys_string> m_interfaceWhitelist;
    
    size_t m_pageSize;
};

#endif 
