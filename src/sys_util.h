// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SYS_UTIL_H_INCLUDED
#define HEADER_SYS_UTIL_H_INCLUDED

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
        auto adminGroup = ptl::Group::getByName(ADMIN_GROUP_NAME);
        return Identity(0, adminGroup ? adminGroup->gr_gid: 0);
#else
        return Identity(0, 0);
#endif
    }

#if CAN_CREATE_USERS

    static auto createDaemonUser(const sys_string & name) -> Identity;

#endif
    
    void setMyIdentity() const {
        ptl::setGroups({});
        ptl::setGid(gid());
        ptl::setUid(uid());
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


inline void createMissingDirs(const std::filesystem::path & path, mode_t mode,
                              std::optional<Identity> owner) {
    
    auto absPath = absolute(path);
    auto start = absPath.root_path();
    
    auto it = absPath.begin();
    std::advance(it, std::distance(start.begin(), start.end()));
    for(auto end = absPath.end(); it != end; ++it) {
        
        start /= *it;
        //we need this check because makeDirectory might fail with things
        //other than EEXIST like permissions
        if (exists(start))
            continue;
        ptl::AllowedErrors<EEXIST> ec; //but we also need this exception to avoid TOCTOU, sigh
        ptl::makeDirectory(start, mode, ec);
        ptl::changeMode(start, mode);
        if (owner)
            ptl::changeOwner(start, owner->uid(), owner->gid());
    }
}


#endif
