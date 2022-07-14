// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_APP_STATE_H_INCLUDED
#define HEADER_APP_STATE_H_INCLUDED

#include "xml_wrapper.h"
#include "command_line.h"
#include "pid_file.h"
#include "config.h"

class AppState {
public:
    AppState(int argc, char ** argv, std::set<int> untouchedSignals);
    
    void reload();
    
    auto config() const -> const refcnt_ptr<Config> {
        return m_config;
    }
    
    auto shouldFork() const -> bool {
        return m_currentCommandLine.chrootDir || m_currentCommandLine.runAs;
    }
    
    
    void preFork();
    
    void postForkInServerProcess() noexcept;

    enum class DaemonStatus {
        Ready,
        Reloading,
        Stopping
    };
    void notify(DaemonStatus status);

private:
    void init();
    void refresh();

    void daemonize();

    void setLogLevel();
    void setLogOutput(bool firstTime);
    void setPidFile();

    static void setLogFile(const std::filesystem::path & filename);
    static void redirectStdFile(const FileDescriptor & fd, FILE * fp);
    static void closeAllExcept(const int * first, const int * last);
private:
    std::set<int> m_untouchedSignals;
    CommandLine m_origCommandLine;
    CommandLine m_currentCommandLine;
    pid_t m_mainPid;
    
    bool m_isInitialized = false;
    std::optional<spdlog::level::level_enum> m_logLevel;
    std::optional<std::filesystem::path> m_logFilePath;
    std::optional<std::filesystem::path> m_pidFilePath;
    PidFile m_pidFile;
    FileDescriptor m_savedStdOut;
    FileDescriptor m_savedStdErr;
    refcnt_ptr<Config> m_config;
};

#endif
