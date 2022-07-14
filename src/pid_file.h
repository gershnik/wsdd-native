// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_PID_FILE_H_INCLUDED
#define HEADER_PID_FILE_H_INCLUDED

#include "sys_util.h"

class PidFile {
public:
    PidFile() = default;
    PidFile(PidFile &&) noexcept = default;
    PidFile & operator=(PidFile && src) noexcept {
        this->~PidFile();
        new (this) PidFile(std::move(src));
        return *this;
    }
    
    static auto open(std::filesystem::path filename,
                     std::optional<Identity> owner = std::nullopt) -> std::optional<PidFile> {

        const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        
        createMissingDirs(filename.parent_path(), mode | S_IXUSR | S_IXGRP | S_IXOTH, owner);
        
        FileDescriptor fd(filename.c_str(), O_WRONLY | O_CREAT, mode);
        if (flock(fd.get(), LOCK_EX | LOCK_NB) != 0) {
            auto err = errno;
            if (err == EWOULDBLOCK)
                return std::nullopt;
            throwErrno("flock()", err);
        }
        changeMode(fd, mode);
        if (owner)
            changeOwner(fd, *owner);
        if (ftruncate(fd.get(), 0) != 0)
            throwErrno("ftruncate()", errno);
        auto pid = getpid();
        auto strPid = std::to_string(pid);
        strPid += '\n';
        if (write(fd.get(), strPid.data(), strPid.size()) != ssize_t(strPid.size()))
            throwErrno("write()", errno);
        return PidFile(std::move(fd), std::move(filename), pid);
    }
    
    ~PidFile() noexcept {
        if (!m_fd)
            return;
        
        if (getpid() != m_lockingProcess)
            return;
        
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
        if (ec)
            WSDLOG_ERROR("unable to remove pidfile {}, error: {}\n", m_path.c_str(), ec.message().c_str());
    }
private:
    PidFile(FileDescriptor && fd, std::filesystem::path && path, pid_t proc) noexcept:
        m_fd(std::move(fd)), m_path(std::move(path)), m_lockingProcess(proc) {
    }
private:
    FileDescriptor m_fd;
	std::filesystem::path m_path;
    pid_t m_lockingProcess = -1;
};

static_assert(!std::is_copy_constructible_v<PidFile>);
static_assert(!std::is_copy_assignable_v<PidFile>);


#endif 
