// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_UTIL_H_INCLUDED
#define HEADER_UTIL_H_INCLUDED

/*
 Portable utilities
 */

#define WSDDN_DECLARE_MEMBER_DETECTOR(type, member, name) \
    struct name##_detector { \
        template<class T> \
        static std::true_type detect(decltype(T::member) *); \
        template<class T> \
        static std::false_type detect(...); \
    }; \
    constexpr bool name = decltype(name##_detector::detect<type>(nullptr))::value

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
    auto format(const NetworkInterface & iface, FormatContext & ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}(idx: {})", iface.name, iface.index);
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

inline sys_string to_urn(const Uuid & val) {
    std::array<char, 36> buf;
    val.to_chars(buf, Uuid::lowercase);

    sys_string_builder builder;
    builder.reserve_storage(46);
    builder.append(S("urn:uuid:"));
    builder.append(buf.data(), buf.size());
    return builder.build();
}

inline sys_string to_sys_string(const Uuid & val) {
    std::array<char, 36> buf;
    val.to_chars(buf, Uuid::lowercase);
    return sys_string(buf.data(), buf.size());
}


template <> struct fmt::formatter<ptl::StringRefArray> {

    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }
    template <typename FormatContext>
    auto format(const ptl::StringRefArray & args, FormatContext & ctx) const -> decltype(ctx.out()) {
        auto dest = ctx.out();
        *dest++ = '[';
        if (auto * str = args.data()) {
            dest = fmt::format_to(dest, "\"{}\"", *str);
            for (++str; *str; ++str) {
                dest = fmt::format_to(dest, ", \"{}\"", *str);
            }
        }
        *dest++ = ']';
        return dest;
    }
};

template<class T, class Arg>
constexpr decltype(auto) makeDependentOn(Arg && arg) {
    return std::forward<Arg>(arg);
}


extern std::mt19937 g_Random; 


#endif
