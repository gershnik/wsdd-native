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

#if __has_include(<version>)
    #include <version>
#endif

#if defined(_LIBCPP_VERSION)

    #if _LIBCPP_VERSION >= 13000 && _LIBCPP_VERSION < 160000
        #undef _LIBCPP_HAS_NO_INCOMPLETE_RANGES
    #endif

#endif

#include <argum/argum.h>

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

#define PTL_USE_STD_FORMAT 0
#include <ptl/file.h>
#include <ptl/signal.h>
#include <ptl/identity.h>
#include <ptl/spawn.h>
#include <ptl/users.h>
#include <ptl/socket.h>
#include <ptl/system.h>
#include <ptl/errors.h>

#include <modern-uuid/uuid.h>

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
#include <chrono>

#include <stdio.h>

#include <sys/mman.h>

#if WSDDN_PLATFORM_APPLE

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>
#include <OpenDirectory/OpenDirectory.h>

#if HAVE_OS_LOG
    #if __has_builtin(__builtin_os_log_format)
        #include <os/log.h>
    #else
        // GCC: os/log.h refuses to compile (needs __builtin_os_log_format).
        // Declare the C ABI we actually call and build the messages by hand.
        extern "C" {
            typedef struct os_log_s *os_log_t;
            typedef uint8_t          os_log_type_t;
            os_log_t os_log_create(const char *subsystem, const char *category);
            void     os_release(void *object);
            void     _os_log_impl(void *dso, os_log_t log, os_log_type_t type,
                                const char *format, uint8_t *buf, uint32_t size);
        }
        #define OS_LOG_TYPE_DEFAULT ((os_log_type_t)0x00)
        #define OS_LOG_TYPE_INFO    ((os_log_type_t)0x01)
        #define OS_LOG_TYPE_DEBUG   ((os_log_type_t)0x02)
        #define OS_LOG_TYPE_ERROR   ((os_log_type_t)0x10)
        #define OS_LOG_TYPE_FAULT   ((os_log_type_t)0x11)
    #endif
#endif

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

using Uuid = muuid::uuid;

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

template<class... Args>
inline
auto sys_format(fmt::format_string<Args...> fmtstr, Args &&... args) -> sys_string {
    sys_string_builder builder;
    fmt::format_to(std::back_inserter(builder.chars()), fmtstr, std::forward<Args>(args)...);
    return builder.build();
}


#endif
