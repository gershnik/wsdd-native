// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_PCH_H_INCLUDED
#define HEADER_PCH_H_INCLUDED

#include <sys_config.h>

#ifdef __GNUC__
    #define WSDDN_SUPPRESS_WARNINGS_BEGIN _Pragma("GCC diagnostic push") 
    #define WSDDN_SUPPRESS_WARNING_HELPER0(arg) #arg
    #define WSDDN_SUPPRESS_WARNING_HELPER1(name) WSDDN_SUPPRESS_WARNING_HELPER0(GCC diagnostic ignored name)
    #define WSDDN_SUPPRESS_WARNING_HELPER2(name) WSDDN_SUPPRESS_WARNING_HELPER1(#name)
    #define WSDDN_SUPPRESS_WARNING(name) _Pragma(WSDDN_SUPPRESS_WARNING_HELPER2(name))
    #define WSDDN_SUPPRESS_WARNINGS_END _Pragma("GCC diagnostic pop")
    
    #define WSDDN_IGNORE_DEPRECATED_BEGIN WSDDN_SUPPRESS_WARNINGS_BEGIN \
        WSDDN_SUPPRESS_WARNING(-Wdeprecated-declarations)
    #define WSDDN_IGNORE_DEPRECATED_END WSDDN_SUPPRESS_WARNINGS_END
#else
    #define WSDDN_SUPPRESS_WARNINGS_BEGIN
    #define WSDDN_SUPPRESS_WARNING(x)
    #define WSDDN_SUPPRESS_WARNINGS_END

    #define WSDDN_IGNORE_DEPRECATED_BEGIN
    #define WSDDN_IGNORE_DEPRECATED_END
#endif

#include <argum/parser.h>
#include <argum/type-parsers.h>
#include <argum/validators.h>

#include <spdlog/fmt/fmt.h>

//must come before sys_string due to S macro collision
#ifdef __clang__
    WSDDN_SUPPRESS_WARNINGS_BEGIN
    WSDDN_SUPPRESS_WARNING(-Wshorten-64-to-32)
#endif
#include <asio.hpp>
#ifdef __clang__
    WSDDN_SUPPRESS_WARNINGS_END
#endif

#include <intrusive_shared_ptr/ref_counted.h>
#include <intrusive_shared_ptr/refcnt_ptr.h>

#include <ptl/file.h>
#include <ptl/signal.h>
#include <ptl/identity.h>
#include <ptl/spawn.h>
#include <ptl/users.h>
#include <ptl/socket.h>
#include <ptl/system.h>
#include <ptl/errors.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/pattern_formatter.h>

#include <sys_string/sys_string.h>

#include <outcome.hpp>

#include <toml++/toml.h>

#if __has_include(<uuid/uuid.h>)
    #include <uuid/uuid.h>
#elif __has_include(<uuid.h>)
    #include <uuid.h>
#else
    #error No uuid.h header available
#endif

#if HAVE_SYSTEMD
    #include <dlfcn.h>
    #include <systemd/sd-daemon.h>
#endif

#include <memory>
#include <stdexcept>
#include <vector>
#include <array>
#include <set>
#include <deque>
#include <optional>
#include <variant>
#include <tuple>
#include <system_error>
#include <random>
#include <filesystem>
#include <regex>

#include <stdio.h>

#if WSDDN_PLATFORM_APPLE

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>
#include <OpenDirectory/OpenDirectory.h>

#include <os/log.h>

#include <intrusive_shared_ptr/apple_cf_ptr.h>

#endif

#define WSDLOG_TRACE(...)    do { if (spdlog::should_log(spdlog::level::trace))    spdlog::trace(__VA_ARGS__);    } while(false)
#define WSDLOG_DEBUG(...)    do { if (spdlog::should_log(spdlog::level::debug))    spdlog::debug(__VA_ARGS__);    } while(false)
#define WSDLOG_INFO(...)     do { if (spdlog::should_log(spdlog::level::info))     spdlog::info(__VA_ARGS__);     } while(false)
#define WSDLOG_WARN(...)     do { if (spdlog::should_log(spdlog::level::warn))     spdlog::warn(__VA_ARGS__);     } while(false)
#define WSDLOG_ERROR(...)    do { if (spdlog::should_log(spdlog::level::err))      spdlog::error(__VA_ARGS__);    } while(false)
#define WSDLOG_CRITICAL(...) do { if (spdlog::should_log(spdlog::level::critical)) spdlog::critical(__VA_ARGS__); } while(false)


using namespace sysstr;
using namespace isptr;

namespace ip = asio::ip;
namespace outcome = OUTCOME_V2_NAMESPACE;


template <> struct fmt::formatter<sys_string> : private fmt::formatter<const char *> {

    using super = fmt::formatter<const char *>;
    using super::parse;

    template <typename FormatContext>
    auto format(const sys_string & str, FormatContext & ctx) const -> decltype(ctx.out()) {
        return super::format(str.c_str(), ctx);
    }
};


#endif
