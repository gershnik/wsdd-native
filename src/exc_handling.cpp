// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "exc_handling.h"

#if __has_include(<execinfo.h>) //moronic Alpine/MUSL have no backtrace and no replacement

#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>

using CodePtr = void (*)(void);

// static auto demangle_free = [](char * ptr) {
//     free(ptr);
// };
// using demangled_string = std::unique_ptr<char, decltype(demangle_free)>;

class Backtrace {
public:
    Backtrace() = default;

    [[gnu::always_inline]] void fill(size_t skip) {

        int res = backtrace(reinterpret_cast<void **>(s_backtraceBuffer.data()), static_cast<int>(s_backtraceBuffer.size()));
        if (res > 0 && size_t(res) > skip) {
            m_pointers.assign(s_backtraceBuffer.begin() + skip, s_backtraceBuffer.begin() + size_t(res) - skip);
        } else {
            m_pointers.clear();
        }
    }
    
    template<class OutIt>
    void print(OutIt dest) const {
        
        fmt::format_to(dest, "--------\n");
        for (auto ptr: m_pointers)
        {
            auto addr = reinterpret_cast</*const*/ void *>(ptr);
            fmt::format_to(dest, "    {:p}", addr);
            Dl_info info{};
            if (dladdr(addr, &info))
            {
                // if (info.dli_sname)
                // {
                //     int stat = 0;
                //     demangled_string demangled(abi::__cxa_demangle(info.dli_sname, 0, 0, &stat), demangle_free);
                //     size_t offset = intptr_t(addr) - intptr_t(info.dli_saddr);
                //     fmt::format_to(dest, " {} + {}", (demangled ? demangled.get() : info.dli_sname), offset);
                // }
                if (info.dli_fname)
                {
                    size_t offset = intptr_t(addr) - intptr_t(info.dli_fbase);
                    const char * basename = strrchr(info.dli_fname, '/');
                    fmt::format_to(dest, " {} + 0x{:X}", (basename ? basename + 1 : info.dli_fname), offset);
                }
            }
            fmt::format_to(dest, "\n");
        }
        fmt::format_to(dest, "--------\n");
    }
private:
    std::vector<CodePtr> m_pointers; 
    static thread_local std::array<CodePtr, 256> s_backtraceBuffer;
};

thread_local std::array<CodePtr, 256> Backtrace::s_backtraceBuffer;

thread_local std::vector<Backtrace> g_backtraces;


extern "C" {
    
#if HAVE_ABI_CXA_THROW
    #define CXA_THROW abi::__cxa_throw
#else
    #define CXA_THROW __cxa_throw
#endif
    
    [[gnu::noinline]] __attribute__((__visibility__("default"))) __attribute__ ((noreturn))
    void CXA_THROW(void * ex, std::type_info * info, void (*dest)(void *)) {

        auto currentIdx = std::uncaught_exceptions();
        g_backtraces.resize(currentIdx + 1);
        g_backtraces[currentIdx].fill(/*skip=*/1);

        using __cxa_throw_t = decltype(&CXA_THROW);

        static __cxa_throw_t __attribute__ ((noreturn)) realCxaThrow = (__cxa_throw_t)dlsym(RTLD_NEXT, "__cxa_throw");
        realCxaThrow(ex, info, dest);
    }
}

template<class OutIt>
static void doPrintCaughtExceptionBacktrace(OutIt dest) {
    auto currentIdx = std::uncaught_exceptions();
    if (size_t(currentIdx) > g_backtraces.size()) {
        fmt::format_to(dest, "    <not available>\n");
        return;
    }
    
    const Backtrace & backtrace = g_backtraces[currentIdx];
    backtrace.print(dest);
}

auto formatCaughtExceptionBacktrace() -> std::string {
    std::string ret = "Backtrace:\n";
    doPrintCaughtExceptionBacktrace(std::back_inserter(ret));
    return ret;
}

#else 

auto formatCaughtExceptionBacktrace() -> std::string {
    return {};
}

#endif
