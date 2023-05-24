// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SYS_UTIL_H_INCLUDED
#define HEADER_SYS_UTIL_H_INCLUDED

[[noreturn]] inline void throwErrno(int err) {
    throw std::system_error(std::make_error_code(static_cast<std::errc>(err)));
}

[[noreturn]] inline void throwErrno(const char * what, int err) {
    throw std::system_error(std::make_error_code(static_cast<std::errc>(err)), what);
}

class FileDescriptor {
public:
    FileDescriptor() noexcept = default;
    explicit FileDescriptor(int fd) noexcept : m_fd(fd) {
    }
    FileDescriptor(const char *path, int oflag, mode_t mode = 0):
        m_fd(::open(path, oflag, mode)){
        
        if (m_fd < 0)
            throwErrno("open()", errno);
    }
    ~FileDescriptor() noexcept {
        if (m_fd >= 0)
            close(m_fd);
    }
    FileDescriptor(FileDescriptor && src) noexcept : m_fd(src.m_fd) {
        src.m_fd = -1;
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor & operator=(FileDescriptor src) noexcept {
        swap(src, *this);
        return *this;
    }

    static auto open(const char *path, int oflag, mode_t mode = 0) noexcept -> outcome::result<FileDescriptor> {

        auto fd = ::open(path, oflag, mode);
        if (fd < 0)
            return std::make_error_code(static_cast<std::errc>(errno));
        return FileDescriptor(fd);
    }
    
    static auto pipe() -> outcome::result<std::pair<FileDescriptor, FileDescriptor>> {
        int fds[2];
        if (::pipe(fds) != 0)
            return std::make_error_code(static_cast<std::errc>(errno));
        return std::make_pair(FileDescriptor(fds[0]), FileDescriptor(fds[1]));
    }
    
    friend void swap(FileDescriptor & lhs, FileDescriptor & rhs) noexcept {
        std::swap(lhs.m_fd, rhs.m_fd);
    }
    
    auto get() const noexcept -> int {
        return m_fd;
    }
    
    auto detach() noexcept -> int {
        int ret = m_fd;
        m_fd = -1;
        return ret;
    }
    
    explicit operator bool() const noexcept {
        return m_fd != -1;
    }
private:
    int m_fd = -1;
};

class Uuid {
private:
    struct IntStorage {
        uint64_t v1;
        uint64_t v2;
    };
    static_assert(sizeof(IntStorage) == sizeof(uuid_t), "IntStorage must much size of uuid_t");
    static_assert(alignof(IntStorage) >= alignof(uuid_t), "IntStorage must have stricter or same alignment as uuid_t");
    
    union ValueType {
        uuid_t system;
        IntStorage optimized;
    };

    using StringType = char[37];
    
public:
    Uuid() {
        m_value.optimized = {0, 0};
    }
    Uuid(const uuid_t & val) {
        uuid_copy(m_value.system, val);
    }

    static auto generate() -> Uuid {
        ValueType val;
        uuid_generate(val.system);
        return val.optimized;
    }

    static auto generateV5(const Uuid & ns, const sys_string & name) -> Uuid {
        ValueType val;
        uuid_generate_sha1(val.system, ns.m_value.system, name.c_str(), name.storage_size());
        return Uuid(val.optimized);
    }
    
    static auto parse(const char * s) -> std::optional<Uuid> {
        
        ValueType val;
        if (uuid_parse(s, val.system) != 0)
            return std::nullopt;
        return Uuid(val.optimized);
    }

    sys_string str() const {
        StringType strUuid;
        uuid_unparse_lower(m_value.system, strUuid);
        return strUuid;
    }

    sys_string urn() const {
        StringType strUuid;
        uuid_unparse_lower(m_value.system, strUuid);
        sys_string_builder builder;
        builder.reserve_storage(46);
        builder.append(S("urn:uuid:"));
        builder.append(strUuid);
        return builder.build();
    }

    friend auto operator<=>(const Uuid & lhs, const Uuid & rhs) -> std::strong_ordering {
        int res = uuid_compare(lhs.m_value.system, rhs.m_value.system);
        switch(res){
            case -1: return std::strong_ordering::less;
            case 0: return std::strong_ordering::equal;
            case 1: return std::strong_ordering::greater;
            default: __builtin_unreachable();
        }
    }

    friend auto operator==(const Uuid & lhs, const Uuid & rhs) -> bool {
        return lhs.m_value.optimized.v1 == rhs.m_value.optimized.v1 &&
               lhs.m_value.optimized.v2 == rhs.m_value.optimized.v2;
    }
    friend auto operator!=(const Uuid & lhs, const Uuid & rhs) -> bool {
        return !(lhs == rhs);
    }
private:
    Uuid(IntStorage storage) {
        m_value.optimized = storage;
    }
private:
    ValueType m_value;
};

class Group : public group {
public:
    Group() {
        memset(static_cast<group *>(this), 0, sizeof(group));
    }
    Group(const Group &) = delete;
    Group(Group &&) = default;
    Group & operator=(const Group &) = delete;
    Group & operator=(Group &&) = default;
    
    static auto getByName(const sys_string & name) -> std::optional<Group> {
        struct group * result = nullptr;
        auto bufSize = std::min(size_t(sysconf(_SC_GETGR_R_SIZE_MAX)), size_t(4096)); 
        Group ret(bufSize);
        for ( ; ; ) {
            int res = getgrnam_r(name.c_str(), &ret, ret.m_buf.data(), ret.m_buf.size(), &result);
            if (res == 0) {
                if (!result)
                    return std::nullopt;
                return ret;
            }
            
            if (auto err = errno; err != ERANGE)
                throwErrno("getgrnam_r()", err);
            ret.m_buf.resize(ret.m_buf.size() + 4096);
        }
    }
    
private:
    Group(size_t buflen): m_buf(buflen) {}
private:
    std::vector<char> m_buf;
};

class Passwd : public passwd {
public:
    Passwd() {
        memset(static_cast<passwd *>(this), 0, sizeof(passwd));
    }
    Passwd(const Passwd &) = delete;
    Passwd(Passwd &&) = default;
    Passwd & operator=(const Passwd &) = delete;
    Passwd & operator=(Passwd &&) = default;
    
    static auto getByName(const sys_string & name) -> std::optional<Passwd> {
        struct passwd * result = nullptr;
        auto bufSize = std::min(size_t(sysconf(_SC_GETPW_R_SIZE_MAX)), size_t(4096));
        Passwd ret(bufSize);
        for ( ; ; ) {
            int res = getpwnam_r(name.c_str(), &ret, ret.m_buf.data(), ret.m_buf.size(), &result);
            if (res == 0) {
                if (!result)
                    return std::nullopt;
                return ret;
            }
            
            if (auto err = errno; err != ERANGE)
                throwErrno("getpwnam_r()", err);
            ret.m_buf.resize(ret.m_buf.size() + 4096);
        }
    }
    
private:
    Passwd(size_t buflen): m_buf(buflen) {}
private:
    std::vector<char> m_buf;
};

class Identity {
public:
    Identity(uid_t uid, gid_t gid) : m_data(uid, gid) {
    }
    
    static auto my() -> Identity {
        return Identity(getuid(), getgid());
    }
    static auto myEffective() -> Identity {
        return Identity(geteuid(), getegid());
    }
    static auto admin() -> Identity {
#ifdef ADMIN_GROUP_NAME
        auto adminGroup = Group::getByName(ADMIN_GROUP_NAME);
        return Identity(0, adminGroup ? adminGroup->gr_gid: 0);
#else
        return Identity(0, 0);
#endif
    }

#if CAN_CREATE_USERS

    static auto createDaemonUser(const sys_string & name) -> Identity;

#endif
    
    void setMyIdentity() const {
        if (setgid(gid()) != 0)
            throwErrno("setgid()", errno);
        if (setuid(uid()) != 0)
            throwErrno("setuid()", errno);
    }

    void setMyEffectiveIdentity() const {
        if (setegid(gid()) != 0)
            throwErrno("setegid()", errno);
        if (seteuid(uid()) != 0)
            throwErrno("seteuid()", errno);
    }
    
    auto uid() const -> uid_t { return m_data.first; }
    auto gid() const -> gid_t { return m_data.second; }
    
private:
    std::pair<uid_t, gid_t> m_data;
};

#if HAVE_SYSTEMD

class SystemdLevelFormatter : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg & msg, const std::tm &, spdlog::memory_buf_t & dest) override
    {
        std::string_view level;
        switch(msg.level) {
            case SPDLOG_LEVEL_CRITICAL:   level = SD_CRIT;    break;
            case SPDLOG_LEVEL_ERROR:      level = SD_ERR;     break;
            case SPDLOG_LEVEL_WARN:       level = SD_WARNING; break;
            case SPDLOG_LEVEL_INFO:       level = SD_INFO;    break;
            default:                      level = SD_DEBUG;   break;
        }
        dest.append(level.data(), level.data() + level.size());
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<SystemdLevelFormatter>();
    }
};

#endif

#if HAVE_OS_LOG

    class OsLogHandle {
    public:
        static auto get() noexcept -> os_log_t { 
            if (!s_handle)
                s_handle = os_log_create(WSDDN_BUNDLE_IDENTIFIER, s_category);
            return s_handle; 
        }
        static void resetInChild() noexcept {
            if (s_handle) {
                os_release(s_handle);
                s_handle = nullptr;
                s_category = "child";
            }
        }
    private:
        static os_log_t s_handle;
        static const char * s_category;
    };

    template<typename Mutex>
    class OsLogSink : public spdlog::sinks::base_sink<Mutex>
    {
        using super = spdlog::sinks::base_sink<Mutex>;
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {

            spdlog::memory_buf_t formatted;
            super::formatter_->format(msg, formatted);
            formatted.append(std::array{'\0'});

            os_log_type_t type;
            switch(msg.level) {
                case SPDLOG_LEVEL_CRITICAL:   type = OS_LOG_TYPE_FAULT;     break;
                case SPDLOG_LEVEL_ERROR:      type = OS_LOG_TYPE_ERROR;     break;
                case SPDLOG_LEVEL_WARN:       type = OS_LOG_TYPE_DEFAULT;   break;
                case SPDLOG_LEVEL_INFO:       type = OS_LOG_TYPE_INFO;      break;
                default:                      type = OS_LOG_TYPE_DEBUG;     break;
            }

            os_log_with_type(OsLogHandle::get(), type, "%{public}s", formatted.data());
        }

        void flush_() override {
        }
    };

#endif

inline void changeOwner(const FileDescriptor & fd, const Identity & owner) {
    if (fchown(fd.get(), owner.uid(), owner.gid()) != 0)
        throwErrno("fchown()", errno);
}

inline void changeOwner(const std::filesystem::path & path, const Identity & owner) {
    if (chown(path.c_str(), owner.uid(), owner.gid()) != 0)
        throwErrno("chown()", errno);
}

inline void changeMode(const FileDescriptor & fd, mode_t mode) {
    if (fchmod(fd.get(), mode) != 0)
        throwErrno("fchmod()", errno);
}

inline void changeMode(const std::filesystem::path & path, mode_t mode) {
    if (chmod(path.c_str(), mode) != 0)
        throwErrno("chmod()", errno);
}


inline auto setSignalHandler(int signo, void (* handler)(int)) -> void (*)(int) {
    auto ret = signal(signo, handler);
    if (ret == SIG_ERR)
        throwErrno("signal()", errno);
    return ret;
}

inline void createMissingDirs(const std::filesystem::path & path, mode_t mode,
                              std::optional<Identity> owner) {
    
    auto absPath = absolute(path);
    auto start = absPath.root_path();
    
    auto it = absPath.begin();
    std::advance(it, std::distance(start.begin(), start.end()));
    for(auto end = absPath.end(); it != end; ++it) {
        
        start /= *it;
        if (exists(start))
            continue;
        if (mkdir(start.c_str(), mode) != 0) {
            auto err = errno;
            if (err != EEXIST)
                throwErrno("mkdir()", err);
        }
        changeMode(start, mode);
        if (owner)
            changeOwner(start, *owner);
    }
}

inline auto signalName(int sig) -> sys_string {
#if HAVE_SIGABBREV_NP
    if (auto str = sigabbrev_np(sig))
        return S("SIG") + sys_string(str);
#elif HAVE_SYS_SIGNAME
    if (sig > 0 && sig < NSIG)
        return S("SIG") + sys_string(sys_signame[sig]).to_upper();
#endif
    
    return std::to_string(sig);
}

inline auto forkProcess() -> pid_t {
    auto ret = fork();
    if (ret < 0)
        throwErrno("fork()", errno);
    return ret;
}


#endif
