// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_UTIL_H_INCLUDED
#define HEADER_UTIL_H_INCLUDED

/*
 Portable utilities
 */

enum AllowedAddressFamily {
    BothIPv4AndIPv6,
    IPv4Only,
    IPv6Only
};

struct WindowsDomain {
    template<class... Args>
    WindowsDomain(Args && ...args): name(std::forward<Args>(args)...) {}
    
    sys_string name;
};

struct WindowsWorkgroup {
    template<class... Args>
    WindowsWorkgroup(Args && ...args): name(std::forward<Args>(args)...) {}
    
    sys_string name;
};

using MemberOf = std::variant<WindowsWorkgroup, WindowsDomain>;

enum class DaemonType {
    Unix
#if HAVE_SYSTEMD
    , Systemd
#endif
#if HAVE_LAUNCHD
    , Launchd
#endif
};

struct NetworkInterface {
    NetworkInterface(int idx, const char * first, const char * last):
        name(first, last),
        index(idx) {
    }
    NetworkInterface(int idx, const sys_string & n):
        name(n),
        index(idx) {
    }
    
    friend auto operator<=>(const NetworkInterface & lhs, const NetworkInterface & rhs) -> std::strong_ordering {
        if (auto res = lhs.name <=> rhs.name; res != 0)
            return res;
        return lhs.index <=> rhs.index;
    }
    friend auto operator==(const NetworkInterface & lhs, const NetworkInterface & rhs) -> bool {
        return lhs.name == rhs.name && lhs.index == rhs.index;
    }
    friend auto operator!=(const NetworkInterface & lhs, const NetworkInterface & rhs) -> bool {
        return !(lhs == rhs);
    }
    
    sys_string name;
    int index;
};

template <> struct fmt::formatter<NetworkInterface> {

    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }
    template <typename FormatContext>
    auto format(const NetworkInterface & iface, FormatContext & ctx) -> decltype(ctx.out()) {
        return format_to(ctx.out(), "{}(idx: {})", iface.name, iface.index);
    }
};

template<class T>
class RefCountedContainerBuffer{
public:
    RefCountedContainerBuffer(T && data):
        m_dataPtr(refcnt_attach(new ref_counted_adapter<T>(std::move(data)))),
        m_buffer(m_dataPtr->data(), m_dataPtr->size()) {

    }
    using value_type = asio::const_buffer;
    using const_iterator = const asio::const_buffer *;

    auto begin() const -> const_iterator { return &m_buffer; }
    auto end() const -> const_iterator { return &m_buffer + 1; }

private:
    refcnt_ptr<ref_counted_adapter<T>> m_dataPtr;
    asio::const_buffer m_buffer;
};


class StdIoFileBase {
public:
    FILE * get() const noexcept {
        return m_fp;
    }
    operator bool() const noexcept {
        return m_fp != nullptr;
    }
protected:
    StdIoFileBase(FILE * fp) : m_fp(fp) {}
    StdIoFileBase(StdIoFileBase && src) noexcept {
        m_fp = src.m_fp;
        src.m_fp = nullptr;
    }
    StdIoFileBase(const StdIoFileBase &) = delete;
    StdIoFileBase & operator=(const StdIoFileBase &) = delete;
    StdIoFileBase & operator=(StdIoFileBase &&) = delete;
    ~StdIoFileBase() noexcept {}

    FILE * m_fp;
};

class StdIoFile : public StdIoFileBase {
public:
    StdIoFile(const char * path, const char * mode, std::error_code & ec) noexcept:
        StdIoFileBase(fopen(path, mode)) {
        
        if (!m_fp) {
            int err = errno;
            ec = std::make_error_code(static_cast<std::errc>(err));
        }
    }
    StdIoFile(StdIoFile && src) noexcept = default;
    auto operator=(StdIoFile && src) noexcept -> StdIoFile & {
        this->~StdIoFile();
        new (this) StdIoFile(std::move(src));
        return *this;
    }
    ~StdIoFile() noexcept {
        if (m_fp)
            fclose(m_fp);
    }
};

class StdIoPipe : public StdIoFileBase {
public:
    StdIoPipe(const char * command, const char * mode, std::error_code & ec):
        StdIoFileBase(popen(command, mode)) {
        if (!m_fp) {
            int err = errno;
            ec = std::make_error_code(static_cast<std::errc>(err));
        }
    }
    StdIoPipe(StdIoPipe && src) noexcept = default;
    auto operator=(StdIoPipe && src) noexcept -> StdIoPipe & {
        this->~StdIoPipe();
        new (this) StdIoPipe(std::move(src));
        return *this;
    }
    ~StdIoPipe() noexcept {
        if (m_fp)
            pclose(m_fp);
    }
};

template<class Sink>
auto readLine(const StdIoFileBase & file, Sink sink) -> outcome::result<bool> {

    for ( ; ; ) {

        int res = fgetc(file.get());
        if (res == EOF) {
            if (ferror(file.get())) 
                return std::make_error_code(static_cast<std::errc>(errno));
            return false;
        }
        auto c = char(res);
        if (c == '\n')
            break;
        sink(c); 
    }
    return true;
}

template<class Sink>
auto readAll(const StdIoFileBase & file, Sink sink) -> outcome::result<void> {
    for ( ; ; ) {

        int res = fgetc(file.get());
        if (res == EOF) {
            if (ferror(file.get()))
                return std::make_error_code(static_cast<std::errc>(errno));
            return outcome::success();
        }
        sink(char(res));
    }
    return outcome::success();
}

inline auto makeHttpUrl(const ip::tcp::endpoint & endp) -> sys_string {
    auto addr = endp.address();
    if (addr.is_v4()) {
        return fmt::format("http://{0}:{1}", addr.to_string(), endp.port());
    } else {
        auto addr6 = addr.to_v6();
        addr6.scope_id(0);
        return fmt::format("http://[{0}]:{1}", addr6.to_string(), endp.port());
    }
}

template<class T, class Arg>
constexpr decltype(auto) makeDependentOn(Arg && arg) {
    return std::forward<Arg>(arg);
}


extern std::mt19937 g_Random; 


#endif
