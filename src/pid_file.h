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
        
        for ( ; ; ) {
            auto fd = ptl::FileDescriptor::open(filename, O_WRONLY | O_CREAT, mode);
            if (!ptl::tryLockFile(fd, ptl::FileLock::Exclusive))
                return std::nullopt;
            struct stat openFileStatus;
            ptl::getStatus(fd, openFileStatus);
            struct stat fileSystemFileStatus;
            std::error_code ec;
            ptl::getStatus(filename, fileSystemFileStatus, ec);
            //if we cannot get the status from filesystem or it is a different file,
            //it means some other process got there before us and created a new pid file
            //and so we need to retry.
            if (ec ||
                openFileStatus.st_dev != fileSystemFileStatus.st_dev ||
                openFileStatus.st_ino != fileSystemFileStatus.st_ino) {

                continue;
            }
            
            auto pid = getpid();
            initFile(fd, mode, owner, pid);
            return PidFile(std::move(fd), std::move(filename), pid);
        }
        
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
    PidFile(ptl::FileDescriptor && fd, std::filesystem::path && path, pid_t proc) noexcept:
        m_fd(std::move(fd)), m_path(std::move(path)), m_lockingProcess(proc) {
    }

    static void initFile(const ptl::FileDescriptor & fd, mode_t mode, const std::optional<Identity> & owner, pid_t pid) {
        ptl::changeMode(fd, mode);
        if (owner)
            ptl::changeOwner(fd, owner->uid(), owner->gid());
        ptl::truncateFile(fd, 0);
        auto strPid = std::to_string(pid);
        strPid += '\n';
        if ((size_t)writeFile(fd, strPid.data(), strPid.size()) != strPid.size())
            throw std::runtime_error("partial write to pid file!");
    }
private:
    ptl::FileDescriptor m_fd;
    std::filesystem::path m_path;
    pid_t m_lockingProcess = -1;
};

static_assert(!std::is_copy_constructible_v<PidFile>);
static_assert(!std::is_copy_assignable_v<PidFile>);


#endif 
