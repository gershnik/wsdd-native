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

    static auto openLogFile(const std::filesystem::path & filename) -> ptl::FileDescriptor;
    static void redirectStdFile(FILE * from, const ptl::FileDescriptor & to);
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
#if HAVE_OS_LOG
    std::optional<bool> m_logToOsLog;
#endif
    PidFile m_pidFile;
    ptl::FileDescriptor m_savedStdOut;
    ptl::FileDescriptor m_savedStdErr;
    refcnt_ptr<Config> m_config;
};

#endif
