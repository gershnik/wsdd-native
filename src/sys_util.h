// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_SYS_UTIL_H_INCLUDED
#define HEADER_SYS_UTIL_H_INCLUDED

#include "util.h"

/*
 OS-dependent (or potentially OS-dependant) utilities
 */


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

template<class Sink>
class LineReader {
public:
    LineReader(size_t maxLineSize, Sink sink):
        m_maxLineSize(maxLineSize),
        m_sink(sink)
    {}

    void operator()(const ptl::FileDescriptor & fd) const {
        std::vector<char> buf;
        buf.reserve(m_maxLineSize);
        bool ignore = false;
        while(true) {
            auto offset = buf.size();
            const size_t addition = std::min(m_maxLineSize - offset, m_maxLineSize);
            assert(addition > 0);
            buf.resize(offset + addition);
            auto read_count = ptl::readFile(fd, buf.data() + offset, addition);
            buf.resize(offset + read_count);
            bool done = (read_count == 0);
            
            auto processed_end = buf.begin();
            for(auto cur = processed_end, end = buf.end(); cur != end; ) {
                if (*cur == '\n') {
                    if (!ignore) {
                        std::string_view line(buf.data() + (processed_end - buf.begin()), cur - processed_end);
                        m_sink(line);
                    }
                    ignore = false;
                    processed_end = ++cur;
                } else {
                    ++cur;
                }
            }
            buf.erase(buf.begin(), processed_end);
            if (buf.size() == m_maxLineSize) {
                WSDLOG_WARN("read line is overly long, ignored");
                ignore = true;
                buf.clear();
            }

            if (done)
                break;
        }
        if (!buf.empty() && !ignore)
            m_sink(std::string_view(buf.data(), buf.size()));
    }
private:
    size_t m_maxLineSize;
    Sink m_sink;
};

void shell(const ptl::StringRefArray & args, bool suppressStdErr, std::function<void (const ptl::FileDescriptor & fd)> reader);


#endif
