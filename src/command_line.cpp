// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "command_line.h"
#include "sys_config.h"
#include "util.h"


using namespace Argum;
using namespace std::literals;

class CommandLine::ConfigFileError : public toml::parse_error {
  
public:
    ConfigFileError(const char * desc, spdlog::level::level_enum lev, const toml::source_region & src):
        parse_error(desc, src),
        level(lev) {
    }
    ConfigFileError(const std::string & str, spdlog::level::level_enum lev, const toml::source_region & src):
        ConfigFileError(str.c_str(), lev, src) {
    }
    
    spdlog::level::level_enum level;
};

static auto parseUser(const sys_string & str) -> Identity {
    
    sys_string username;
    std::optional<sys_string> groupname;
    
    if (auto split = str.partition_at_first(U':')) {
        groupname = split->first;
        username = split->second;
    } else {
        username = str;
    }
    
    gid_t gid;
    uid_t uid;
    
    auto pwd = ptl::Passwd::getByName(username);
    if (!pwd)
        throw Parser::ValidationError(fmt::format("non-existent user {}", username));
    uid = pwd->pw_uid;
    if (!groupname) {
        gid = pwd->pw_gid;
    } else {
        auto grp = ptl::Group::getByName(*groupname);
        if (!grp)
            throw Parser::ValidationError(fmt::format("non-existent group {}", *groupname));
        gid = grp->gr_gid;
    }
    
    return Identity(uid, gid);
}

static auto addInterface(CommandLine & cmdline, sys_string val) {
    
    if (val.empty())
        throw Parser::ValidationError("interface name cannot be empty");
    cmdline.interfaces.emplace_back(val);
}

static auto setUuid(CommandLine & cmdline, sys_string val) {
    if (auto maybeUuid = Uuid::from_chars(std::span(val.c_str(), val.storage_size())))
        cmdline.uuid = *maybeUuid;
    throw Parser::ValidationError("invalid uuid");
}

static auto setAllowedAddressFamily(CommandLine & cmdline, sys_string val) {
    sys_string family = val.to_lower();
    if (family == S("ipv4"))
        cmdline.allowedAddressFamily = IPv4Only;
    else if (family == S("ipv6"))
        cmdline.allowedAddressFamily = IPv6Only;
    else if (family == S("both"))
        cmdline.allowedAddressFamily = BothIPv4AndIPv6;
    else
        throw Parser::ValidationError("allowed-address-families must be one of: IPv4, IPv6 or Both");
}

static auto setHostname(CommandLine & cmdline, sys_string val) {
    if (val.empty())
        throw Parser::ValidationError("hostname cannot be empty");
    if (val == S(":NETBIOS:"))
        cmdline.hostname.emplace();
    else
        cmdline.hostname.emplace(std::move(val));
}

static auto setDomain(CommandLine & cmdline, sys_string val) {
    if (val.empty())
        throw Parser::ValidationError("domain cannot be empty");
    cmdline.memberOf.emplace(WindowsDomain(std::move(val)));
}

static auto setWorkgroup(CommandLine & cmdline, sys_string val) {
    if (val.empty())
        throw Parser::ValidationError("workgroup cannot be empty");
    cmdline.memberOf.emplace(WindowsWorkgroup(std::move(val)));
}

static auto setMemberOf(CommandLine & cmdline, sys_string val) {
    auto maybePartition = val.partition_at_first(U'/');
    if (maybePartition) {
        auto [prefix, suffix] = *maybePartition;
        sys_string name = suffix.trim();
        if (name.empty())
            throw Parser::ValidationError("invalid name");
        sys_string type = prefix.to_lower();
        if (type == S("workgroup"))
            cmdline.memberOf.emplace(WindowsWorkgroup(std::move(name)));
        else if (type == S("domain"))
            cmdline.memberOf.emplace(WindowsDomain(std::move(name)));
        else
            throw Parser::ValidationError("invalid member-of prefix");
    } else {
        auto name = val.trim().to_upper();
        if (name.empty())
            throw Parser::ValidationError("member-of value cannot be empty");
        cmdline.memberOf.emplace(WindowsWorkgroup(std::move(name)));
    }
}

#if CAN_HAVE_SAMBA
static auto setSmbConf(CommandLine & cmdline, std::string_view val) {
    if (val.empty())
        throw Parser::ValidationError("smb.conf path cannot be empty");
    auto value = absolute(std::filesystem::path(val));
    if (!exists(value))
        throw Parser::ValidationError("smb.conf path does not exist");
    cmdline.smbConf.emplace(std::move(value));
}
#endif

static auto setMetadataFile(CommandLine & cmdline, std::string_view val) {
    if (val.empty())
        throw Parser::ValidationError("metadata path cannot be empty");
    auto value = absolute(std::filesystem::path(val));
    if (!exists(value))
        throw Parser::ValidationError("metadata path does not exist");
    cmdline.metadataFile.emplace(std::move(value));
}

static auto setLogLevel(CommandLine & cmdline, unsigned decrease) {
    unsigned maxLevel = spdlog::level::level_enum::n_levels - 1;
    if (decrease > maxLevel)
        decrease = maxLevel;
    auto level = spdlog::level::level_enum(maxLevel - decrease);
    cmdline.logLevel = level;
}

static auto setLogFile(CommandLine & cmdline, std::string_view val) {
    if (val.empty())
        throw Parser::ValidationError("log file path cannot be empty");
#if HAVE_OS_LOG
    if (cmdline.logToOsLog && *cmdline.logToOsLog)
        throw Parser::ValidationError("logging to file and system log are mutually exclusive");
#endif
    auto value = absolute(std::filesystem::path(val));
    cmdline.logFile.emplace(std::move(value));
}

#if HAVE_OS_LOG

static auto setLogToOsLog(CommandLine & cmdline, bool val) {
    if (val && cmdline.logFile)
        throw Parser::ValidationError("logging to file and system log are mutually exclusive");
    cmdline.logToOsLog = val;
}

#endif

static auto setPidFile(CommandLine & cmdline, std::string_view val) {
    if (val.empty())
        throw Parser::ValidationError("pid file path cannot be empty");
    auto value = absolute(std::filesystem::path(val));
    cmdline.pidFile.emplace(std::move(value));
}

static auto setRunAs(CommandLine & cmdline, std::string_view val) {
    if (getuid() != 0)
        throw Parser::ValidationError("you must start " WSDDN_PROGNAME " as root to use different username");
    if (val.empty())
        throw Parser::ValidationError("username cannot be empty");
    cmdline.runAs.emplace(parseUser(val));
}

static auto setChrootDir(CommandLine & cmdline, std::string_view val) {
    if (getuid() != 0)
        throw Parser::ValidationError("you must start " WSDDN_PROGNAME " as root to use chroot path");
    if (val.empty())
        throw Parser::ValidationError("chroot path cannot be empty");
    auto value = absolute(std::filesystem::path(val));
    if ( !exists(value))
        throw Parser::ValidationError("chroot path does not exist");
    cmdline.chrootDir.emplace(std::move(value));
}

static std::string colorTagged(std::string_view str) {

    static constexpr std::pair<std::string_view, std::string_view> replacements[] = {
        {"norm", makeColor<Color::normal>()},
        {"bold", makeColor<Color::bold>()},
        {"longopt", defaultColorScheme().longOptionInUsage},
        {"arg", defaultColorScheme().optionArgInUsage},
    };

    static const std::regex patterns_re = []() {
        std::string patterns = "\\{(?:";
        patterns += "(";
        patterns += replacements[0].first;
        patterns += ")";
        for (size_t i = 1; i < std::size(replacements); ++i) {
            patterns += "|(";
            patterns += replacements[i].first;
            patterns += ")";
        }
        patterns += ")\\}";

        return std::regex(patterns, std::regex_constants::ECMAScript);
    }();
    
    std::string ret;
    for (auto start = str.cbegin(); ; ) {
        std::cmatch m;
        if (!std::regex_search(start, str.cend(), m, patterns_re)) {
            ret.append(start, str.cend());
            break;
        }
        size_t found_idx = std::find_if(m.begin() + 1, m.end(), [](auto & sm) {
            return sm.length() > 0;
        }) - m.begin();
        if (found_idx < m.size() && found_idx - 1 < std::size(replacements)) {
            ret.append(start, m[0].first);
            ret.append(replacements[found_idx - 1].second);
        } else {
            ret.append(start, m[0].second);
        }
        start = m[0].second;
    }
    return ret;
}

static void removeColors(std::string & str) {
    enum {
        stateNormal,
        stateEsc,
        stateControlStart,
        stateControlIntermediate
    } state = stateNormal;
    size_t putIdx = 0;
    for (char c: str) {
        switch(state) {
        break; case stateNormal: restart:
            if (c == '\x1b') {
                state = stateEsc;
                continue;
            }
            str[putIdx++] = c;

        break; case stateEsc:
            if (c == '[') {
                state = stateControlStart;
                continue;
            }
            state = stateNormal;
            goto restart;

        break; case stateControlStart:
            if (c >= 0x30 && c <= 0x3F) {
                continue;
            }
            if (c >= 0x20 && c <= 0x2F) {
                state = stateControlIntermediate;
                continue;
            }
            if (c >= 0x40 && c <= 0x7E) {
                state = stateNormal;
                continue;
            }
            state = stateNormal;
            goto restart;

        break; case stateControlIntermediate:
            if (c >= 0x20 && c <= 0x2F) {
                state = stateControlIntermediate;
                continue;
            }
            if (c >= 0x40 && c <= 0x7E) {
                state = stateNormal;
                continue;
            }
            state = stateNormal;
            goto restart;
        }
    }
    str.resize(putIdx);
}

void CommandLine::parse(int argc, char * argv[], ColorStatus envColorStatus) {
 
    const char * const progname = (argc ? argv[0] : WSDDN_PROGNAME);
    
    Argum::Parser parser;
    //Program options
    parser.add(Option("--help", "-h").
               help("show this help message and exit").
               handler([&]() {

        auto useColor = shouldUseColor(envColorStatus, stdout);
        auto colorizer = colorizerForFile(envColorStatus, stdout);
        auto help = parser.formatHelp(progname, terminalWidth(stdout), colorizer);
        if (!useColor) 
            removeColors(help);
        fmt::print("{}", help);
        exit(EXIT_SUCCESS);
    }));
    parser.add(Option("--version", "-v").
               help("report app version and exit").
               handler([&]() {
        fmt::print("{}\n", WSDDN_VERSION);
        exit(EXIT_SUCCESS);
    }));
    parser.add(Option("--config", "-c").
               argName("PATH").
               help("path to config file").
               handler([&](std::string_view val) {
        if (val.empty())
            throw Parser::ValidationError("--config path cannot be empty");
        this->configFile.emplace(val);
    }));
    parser.add(Option("--unixd").
               help("run as a traditional UNIX daemon").
               handler([&]() {
        this->daemonType.emplace(DaemonType::Unix);
    }));
#if HAVE_SYSTEMD
    parser.add(Option("--systemd").
               help("run as a systemd daemon").
               handler([&]() {
        this->daemonType.emplace(DaemonType::Systemd);
    }));
#endif
#if HAVE_LAUNCHD
    parser.add(Option("--launchd").
               help("run as a launchd daemon").
               handler([&]() {
        this->daemonType.emplace(DaemonType::Launchd);
    }));
#endif
    
    //Network options
    parser.add(Option("--interface", "-i").
               argName("NAME").
               help("interface to use. Pass this option multiple times for multiple interfaces").
               handler([this](std::string_view arg) {
        addInterface(*this, sys_string(arg).trim());
    }));
    parser.add(Option("--ipv4only", "-4").
               help("use only IPv4. By default both IPv4 and IPv6 are used.").
               handler([this](){
        this->allowedAddressFamily = IPv4Only;
    }));
    parser.add(Option("--ipv6only", "-6").
               help("use only IPv6. By default both IPv4 and IPv6 are used.").
               handler([this](){
        this->allowedAddressFamily = IPv6Only;
    }));
    parser.addValidator(oneOrNoneOf(optionPresent("--ipv4only"), optionPresent("--ipv6only")),
                        "--ipv4only and --ipv6only cannot be used together");
    parser.add(Option("--hoplimit").
               argName("NUMBER").
               help(colorTagged("hop limit for multicast packets (default = {bold}1{norm})")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        this->hoplimit = Argum::parseIntegral<unsigned>(val);
    }));
    parser.add(Option("--source-port").
               help("send multicast traffic and receive replies on this port.").
               handler([this](std::string_view val){
        this->sourcePort = Argum::parseIntegral<unsigned>(val);
    }));
    
    //Machine info
    parser.add(Option("--uuid").
               argName("UUID").
               help("fixed identifier for this computer. If not specified it will be auto-generated").
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val) {
        setUuid(*this, sys_string(val));
    }));
    parser.add(Option("--hostname", "-H").
               argName("NAME").
               help(colorTagged(
                    "override hostname to be reported to Windows machines. "
                    "If you set the value to {bold}\":NETBIOS:\"{norm} then Netbios hostname will be used. "
                    "The Netbios hostname is either detected from SMB configuration, if found, or produced "
                    "by capitalizing normal machine hostname.")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val) {
        setHostname(*this, sys_string(val).trim());
    }));
    parser.add(Option("--domain", "-D").
               argName("NAME").
               help(colorTagged(
                    "report this computer as a member of Windows domain {arg}NAME{norm}.\n"
                    "{longopt}--domain{norm} and {longopt}--workgroup{norm} are mutually exclusive. "
                    "If neither is specified domain/workgroup membership is auto-detected")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setDomain(*this, sys_string(val).trim());
    }));
    parser.add(Option("--workgroup", "-W").
               argName("NAME").
               help(colorTagged(
                    "report this computer as a member of Windows workgroup {arg}NAME{norm}.\n"
                    "{longopt}--domain{norm} and {longopt}--workgroup{norm} are mutually exclusive. "
                    "If neither is specified domain/workgroup membership is auto-detected")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setWorkgroup(*this, sys_string(val).trim());
    }));
    parser.addValidator(oneOrNoneOf(optionPresent("--domain"), optionPresent("--workgroup")),
                        "--domain and --workgroup cannot be used together");
#if CAN_HAVE_SAMBA
    parser.add(Option("--smb-conf").
               argName("PATH").
               help(colorTagged(
                    "location of {bold}smb.conf{norm} (a.k.a. {bold}samba.conf{norm}) to get the workgroup etc. information from. "
                    "Normally its location is auto-detected. Use this option if auto-detection fails or picks wrong samba instance.")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setSmbConf(*this, val);
    }));
#endif
    
    parser.add(Option("--metadata", "-m").
               argName("PATH").
               help("location of a custom metadata XML file").
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setMetadataFile(*this, val);
    }));
    
    //Behavior options
    parser.add(Option("--log-level").
               argName("LEVEL").
               help(colorTagged(
                    "set log level (default = {bold}4{norm}). Log levels range from "
                    "{bold}0{norm} (disable logging) to {bold}6{norm} (detailed trace).\n"
                    "Passing values bigger than {bold}6{norm} is equivalent to {bold}6{norm}")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        auto decrease = Argum::parseIntegral<unsigned>(val);
        setLogLevel(*this, decrease);
    }));
    parser.add(Option("--log-file").
               argName("PATH").
               help(colorTagged(
                    "the file to write the log output to.\n"
                    "If {longopt}--user{norm} option is used, the directory of the logfile must allow the specified "
                    "user to create and delete files")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setLogFile(*this, val);
    }));
#if HAVE_OS_LOG
    parser.add(Option("--log-os-log").
               help(colorTagged(
                    "log to system log.\n"
                    "This option is mutually exclusive with {longopt}--log-file{norm}")).
               occurs(Argum::neverOrOnce).
               handler([this](){
        setLogToOsLog(*this, true);
    }));
#endif
    parser.add(Option("--pid-file").
               argName("PATH").
               help(colorTagged(
                    "PID file to create.\n"
                    "If {longopt}--user{norm} option is used, the directory of the pidfile must "
                    "allow the specified user to create and delete files")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setPidFile(*this, val);
    }));
    parser.add(Option("--user", "-U").
               argName("USERNAME").
               help(colorTagged(
                    "the account to run networking code under. "
                    "{arg}USERNAME{norm} can be either a plain user or in "
                    "a {bold}groupname:username{norm} form")).
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setRunAs(*this, val);
    }));
    parser.add(Option("--chroot", "-r").
               argName("DIR").
               help("chroot to specified directory").
               occurs(Argum::neverOrOnce).
               handler([this](std::string_view val){
        setChrootDir(*this, val);
    }));
    
    try {
        parser.parse(argc, argv);
    } catch (ParsingException & ex) {
        auto colorizer = colorizerForFile(envColorStatus, stderr);
        fmt::print(stderr, "{}\n\n", colorizer.error(ex.message()));
        fmt::print(stderr, "{}", parser.formatUsage(progname, terminalWidth(stderr), colorizer));
        exit(EXIT_FAILURE);
    }
}

void CommandLine::parseConfigKey(std::string_view keyName, const toml::node & value) {
    
    //Network options
    if (keyName == "interfaces"sv) {
        
        setConfigValue<toml::array>(!this->interfaces.empty(), keyName, value, [this](const toml::array & val) {
            
            for(auto & el : val) {
                auto * name = el.as_string();
                if (!name) {
                    auto & source = val.source();
                    WSDLOG_WARN("{}, line {}, col: {}: interface names must be strings, ignoring", *source.path, source.begin.line, source.begin.column);
                    continue;
                }
                addInterface(*this, sys_string(name->get()).trim());
            }
        });
        
    } else if (keyName == "allowed-address-family"sv) {
        
        setConfigValue<std::string>(bool(this->allowedAddressFamily), keyName, value, [this](const toml::value<std::string> & val) {
            setAllowedAddressFamily(*this, sys_string(*val));
        });
        
    } else if (keyName == "hoplimit"sv) {
        
        setConfigValue<int64_t>(bool(this->hoplimit), keyName, value, [this](const toml::value<int64_t> & val) {
            if (*val < 1)
                throw ConfigFileError("hoplimit value must be greater than 0", spdlog::level::err, val.source());
            this->hoplimit = int((unsigned short)*val);
        });
        
    } else if (keyName == "source-port"sv) {
        
        setConfigValue<int64_t>(bool(this->sourcePort), keyName, value, [this](const toml::value<int64_t> & val) {
            if (*val < 0 || *val >= 65536)
                throw ConfigFileError("source-port value must be in [0, 65536) range", spdlog::level::err, val.source());
            this->sourcePort = uint16_t(*val);
        });
        
    } else
        
    //Machine info
    
    if (keyName == "uuid"sv) {
        
        setConfigValue<std::string>(bool(this->uuid), keyName, value, [this](const toml::value<std::string> & val) {
            setUuid(*this, sys_string(*val));
        });
        
    } else if (keyName == "hostname"sv) {
        
        setConfigValue<std::string>(bool(this->hostname), keyName, value, [this](const toml::value<std::string> & val) {
            setHostname(*this, sys_string(*val).trim());
        });
    
    } else if (keyName == "member-of"sv) {
        
        setConfigValue<std::string>(bool(this->memberOf), keyName, value, [this](const toml::value<std::string> & val){
            setMemberOf(*this, sys_string(*val));
        });
        
    } else if (keyName == "smb-conf"sv) {
    #if CAN_HAVE_SAMBA
        setConfigValue<std::string>(bool(this->smbConf), keyName, value, [this](const toml::value<std::string> & val) {
            setSmbConf(*this, *val);
        });
    #else
        throw ConfigFileError("smb.conf parsing is not available on Mac", spdlog::level::err, value.source());
    #endif
        
    } else if (keyName == "metadata"sv) {
        
        setConfigValue<std::string>(bool(this->metadataFile), keyName, value, [this](const toml::value<std::string> & val) {
            setMetadataFile(*this, *val);
        });
    
    } else
    
    //Behavior options
    if (keyName == "log-level"sv) {
        
        setConfigValue<int64_t>(bool(this->logLevel), keyName, value, [this](const toml::value<int64_t> & val) {
            if (*val < 0)
                throw ConfigFileError("log-level value must be greater or equal to 0", spdlog::level::err, val.source());
            auto decrease = unsigned(*val);
            setLogLevel(*this, decrease);
        });
        
    } else if (keyName == "log-file"sv) {
        
        setConfigValue<std::string>(bool(this->logFile), keyName, value, [this](const toml::value<std::string> & val) {
            setLogFile(*this, *val);
        });
        
#if HAVE_OS_LOG

    } else if (keyName == "log-os-log"sv) {
        
        setConfigValue<bool>(bool(this->logToOsLog), keyName, value, [this](const toml::value<bool> & val) {
            setLogToOsLog(*this, *val);
        });
#endif

    } else if (keyName == "pid-file"sv) {
        
        setConfigValue<std::string>(bool(this->pidFile), keyName, value, [this](const toml::value<std::string> & val) {
            setPidFile(*this, *val);
        });
        
    } else if (keyName == "user"sv) {
        
        setConfigValue<std::string>(bool(this->runAs), keyName, value, [this](const toml::value<std::string> & val) {
            setRunAs(*this, *val);
        });
        
    } else if (keyName == "chroot"sv) {
        
        setConfigValue<std::string>(bool(this->chrootDir), keyName, value, [this](const toml::value<std::string> & val) {
            setChrootDir(*this, *val);
        });
        
    } else {
        throw ConfigFileError(fmt::format("invalid key {}", keyName), spdlog::level::err, value.source());
    }
}

template<class Expected, class Handler>
void CommandLine::setConfigValue(bool isSet, std::string_view keyName, const toml::node & value, Handler handler) {
    
    if (isSet) {
        auto & source = value.source();
        WSDLOG_WARN("{}, line {}: {} value is already set on command line, ignoring", *source.path, source.begin.line, keyName);
        return;
    }
    auto * val = value.as<Expected>();
    if (!val) {
        if constexpr (std::is_same_v<Expected, bool>)
            throw ConfigFileError(fmt::format("{} must be true or false", keyName), spdlog::level::err, value.source());
        else if constexpr (std::is_same_v<Expected, int64_t>)
            throw ConfigFileError(fmt::format("{} must be an integer", keyName), spdlog::level::err, value.source());
        else if constexpr (std::is_same_v<Expected, std::string>)
            throw ConfigFileError(fmt::format("{} must be a string", keyName), spdlog::level::err, value.source());
        else if constexpr (std::is_same_v<Expected, toml::array>)
            throw ConfigFileError(fmt::format("{} must be an array", keyName), spdlog::level::err, value.source());
        else
            static_assert(makeDependentOn<Expected>(false), "type not handled");
    }
    handler(*val);
}

void CommandLine::mergeConfigFile(const std::filesystem::path & path) {
 
    std::error_code ec;
    auto file = ptl::FileDescriptor::open(path.c_str(), O_RDONLY, ec);
    if (ec) {
        WSDLOG_WARN("Cannot open config file {}, error: {}", path.c_str(), ec.message());
        return;
    }

    struct ::stat st;
    getStatus(file, st, ec);
    if (ec) {
        WSDLOG_WARN("Cannot stat config file {}, error: {}", path.c_str(), ec.message());
        return;
    }

    ptl::MemoryMap content(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, file, 0, ec);
    if (ec) {
        WSDLOG_WARN("Cannot map config file {}, error: {}", path.c_str(), ec.message());
        return;
    }
    
    try {
        auto cfg = toml::parse(std::string_view((const char *)content.data(), content.size()), path.string());
        
        for (auto && [key, value] : cfg) {
            
            try {
                
                parseConfigKey(key.str(), value);
                
            } catch (CommandLine::ConfigFileError & ex) {
                
                auto & source = ex.source();
                if (spdlog::should_log(ex.level))
                    spdlog::log(ex.level, "{}, line {}: {}, ignoring", *source.path, source.begin.line, ex.description());
                
            } catch (toml::parse_error & ex) {
                
                auto & source = ex.source();
                WSDLOG_ERROR("{}, line {}: {}, ignoring", *source.path, source.begin.line, ex.description());
                
            } catch (Parser::ValidationError & ex) {
                
                auto & source = value.source();
                WSDLOG_ERROR("{}, line {}: {}, ignoring", *source.path, source.begin.line, ex.message());
            }
        }
        
    } catch (toml::parse_error & ex) {
        auto & source = ex.source();
        WSDLOG_ERROR("{}, line {}: {}", *source.path, source.begin.line, ex.description());
    }
}
