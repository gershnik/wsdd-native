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
    m_sourcePort = cmdline.sourcePort.value_or(0);

    m_fullHostName = getHostName();
    m_simpleHostName = m_fullHostName.prefix_before_first(U'.').value_or(m_fullHostName);

    if (cmdline.uuid) {
        m_uuid = *cmdline.uuid;
    } else {
        using namespace muuid;
        m_uuid = uuid::generate_sha1(uuid("49DAC291-0608-41C9-941C-ED0E7ACCDE1E"), 
                                     {m_fullHostName.c_str(), m_fullHostName.storage_size()});
    }
    m_strUuid = to_sys_string(m_uuid);
    m_urnUuid = to_urn(m_uuid);
        
    bool useNetbiosHostName = cmdline.hostname && cmdline.hostname->empty();
        
    std::optional<WinNetInfo> systemWinNetInfo;
#if HAVE_APPLE_SAMBA
    systemWinNetInfo = detectWinNetInfo(useNetbiosHostName);
#elif CAN_HAVE_SAMBA
    systemWinNetInfo = detectWinNetInfo(cmdline.smbConf, useNetbiosHostName);
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
    
    if (m_winNetInfo.hostDescription.empty()) {
        if (cmdline.hostname && !cmdline.hostname->empty())
            m_winNetInfo.hostDescription = *cmdline.hostname;
        else
            m_winNetInfo.hostDescription = m_simpleHostName;
    }
    
    
    auto [memberOfType, memberOfName] = std::visit([](auto & val) {
        
        using ArgType = std::remove_cvref_t<decltype(val)>;
        
        if constexpr (std::is_same_v<WindowsWorkgroup, ArgType>)
            return std::make_pair(S("Workgoup"), val.name);
        else if constexpr (std::is_same_v<WindowsDomain, ArgType>)
            return std::make_pair(S("Domain"), val.name);
        else
            static_assert(makeDependentOn<ArgType>(false), "unhandled type");
        
    }, m_winNetInfo.memberOf);
        
    if (cmdline.metadataFile) {
        m_metadataDoc = loadMetadaFile(cmdline.metadataFile->native());
    }

    WSDLOG_INFO("Configuration:\n"
                "    Hostname: {}\n"
                "    {}: {}\n"
                "    Description: {}\n"
                "    Identifier: {}\n"
                "    Metadata: {}",
                m_winNetInfo.hostName,
                memberOfType, memberOfName,
                m_winNetInfo.hostDescription,
                endpointIdentifier(),
                m_metadataDoc ? cmdline.metadataFile->c_str() : "default");
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

auto Config::loadMetadaFile(const std::string & filename) const -> std::unique_ptr<XmlDoc> {
    auto file = ptl::FileDescriptor::open(filename, O_RDONLY);
    std::vector<uint8_t> buf(m_pageSize);
    try {
        auto read = readFile(file, buf.data(), buf.size());
        if (read < 4)
            throw std::runtime_error(fmt::format("metada file {} is invalid", filename));
        auto templateParsingCtx = XmlParserContext::createPush(buf.data(), int(read), filename.c_str());
        for ( ; ; ) {
            read = readFile(file, buf.data(), buf.size());
            templateParsingCtx->parseChunk(buf.data(), int(read), read == 0);
            if (!read) {
                break;
            }
        }
        
        if (!templateParsingCtx->wellFormed())
            throw std::runtime_error(fmt::format("metada file {} is not well formed XML", filename));
        return templateParsingCtx->extractDoc();
        
    } catch (XmlException & ex) {
        throw std::runtime_error(fmt::format("metada file {} is not a valid XML", filename));
    }
    
}

