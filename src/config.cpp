// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "config.h"
#include "command_line.h"

Config::Config(const CommandLine & cmdline):
    m_instanceIdentifier(time(nullptr)),
    m_pageSize(size_t(ptl::systemConfig(_SC_PAGESIZE).value_or(4096))) {
        
    m_hopLimit = cmdline.hoplimit.value_or(1);
    m_allowedAddressFamily = cmdline.allowedAddressFamily.value_or(BothIPv4AndIPv6);
    m_interfaceWhitelist.insert(cmdline.interfaces.begin(), cmdline.interfaces.end());

    m_fullHostName = getHostName();
    m_simpleHostName = m_fullHostName.prefix_before_first(U'.').value_or(m_fullHostName);

    if (cmdline.uuid) {
        m_uuid = *cmdline.uuid;
    } else {
        static Uuid ns = Uuid::parse("49DAC291-0608-41C9-941C-ED0E7ACCDE1E").value();
        m_uuid = Uuid::generateV5(ns, m_fullHostName);
    }
    m_strUuid = m_uuid.str();
    m_urnUuid = m_uuid.urn();
        
    bool useNetbiosHostName = cmdline.hostname && cmdline.hostname->empty();
        
    std::optional<WinNetInfo> systemWinNetInfo;
#if HAVE_APPLE_SAMBA
    systemWinNetInfo = detectWinNetInfo(useNetbiosHostName);
#elif CAN_HAVE_SAMBA
    auto smbConf = cmdline.smbConf;
    if (!smbConf)
        smbConf = findSmbConf();
    if (smbConf)
        systemWinNetInfo = readSmbConf(*smbConf, useNetbiosHostName);
#endif
       
    if (cmdline.memberOf) {
        m_winNetInfo.memberOf = *cmdline.memberOf;
    } else if (systemWinNetInfo) {
        m_winNetInfo.memberOf = systemWinNetInfo->memberOf;
    } else {
        m_winNetInfo.memberOf.emplace<WindowsWorkgroup>(S("WORKGROUP"));
    }
        
    if (cmdline.hostname && !cmdline.hostname->empty()) {
        //explict hostname specified
        m_winNetInfo.hostName = *cmdline.hostname;
    } else if (systemWinNetInfo) {
        //we have detected hostname
        m_winNetInfo.hostName = systemWinNetInfo->hostName;
    } else {
        if (useNetbiosHostName)
            m_winNetInfo.hostName = m_simpleHostName.to_upper();
        else
            m_winNetInfo.hostName = m_simpleHostName;
    }
        
    if (systemWinNetInfo)
        m_winNetInfo.hostDescription = systemWinNetInfo->hostDescription;
    if (cmdline.hostname && !cmdline.hostname->empty())
        m_winNetInfo.hostDescription = *cmdline.hostname;
    else
        m_winNetInfo.hostDescription = m_simpleHostName;
    
    
    auto [memberOfType, memberOfName] = std::visit([](auto & val) {
        
        using ArgType = std::remove_cvref_t<decltype(val)>;
        
        if constexpr (std::is_same_v<WindowsWorkgroup, ArgType>)
            return std::make_pair(S("Workgoup"), val.name);
        else if constexpr (std::is_same_v<WindowsDomain, ArgType>)
            return std::make_pair(S("Domain"), val.name);
        else
            static_assert(makeDependentOn<ArgType>(false), "unhandled type");
        
    }, m_winNetInfo.memberOf);
        
    WSDLOG_INFO("Configuration:\n    Hostname: {}\n    {}: {}\n    Description: {}\n    Identifier: {}",
                 m_winNetInfo.hostName,
                 memberOfType, memberOfName,
                 m_winNetInfo.hostDescription,
                 endpointIdentifier());
}

auto Config::getHostName() const -> sys_string {
    auto size = size_t(ptl::systemConfig(_SC_HOST_NAME_MAX).value_or(_POSIX_HOST_NAME_MAX));
    sys_string_builder builder;
    auto & buf = builder.chars();
    buf.resize(size + 1);
    ptl::getHostName({buf.begin(), buf.end()});
    builder.resize_storage(strlen(buf.begin()));
    return builder.build();
}

