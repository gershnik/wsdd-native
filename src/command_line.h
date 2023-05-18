// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_COMMAND_LINE_H_INCLUDED
#define HEADER_COMMAND_LINE_H_INCLUDED

#include "sys_util.h"
#include "util.h"

struct CommandLine {
    std::optional<std::filesystem::path> configFile;
    std::optional<DaemonType> daemonType;
    
    std::vector<sys_string> interfaces;
    std::optional<AllowedAddressFamily> allowedAddressFamily;
    std::optional<int> hoplimit;
    
    std::optional<Uuid> uuid;
    std::optional<sys_string> hostname;
    std::optional<MemberOf> memberOf;
    
#if CAN_HAVE_SAMBA
    std::optional<std::filesystem::path> smbConf;
#endif
    
    std::optional<spdlog::level::level_enum> logLevel;
    std::optional<std::filesystem::path> logFile;
#if HAVE_OS_LOG
    std::optional<bool> logToOsLog;
#endif
    std::optional<std::filesystem::path> pidFile;
    std::optional<Identity> runAs;
    std::optional<std::filesystem::path> chrootDir;
    

    void parse(int argc, char * argv[]);
    void mergeConfigFile(const std::filesystem::path & path);
    
private:
    class ConfigFileError;
    
    template<class Handler>
    void parseConfigKey(std::string_view keyName, const toml::node & value, Handler h);
    
    void parseConfigKey(std::string_view keyName, const toml::node & value);
    
    template<class Expected, class Handler>
    void setConfigValue(bool isSet, std::string_view keyName, const toml::node & value, Handler handler);
};

#endif

