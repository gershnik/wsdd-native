// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "app_state.h"

AppState::AppState(int argc, char ** argv, std::set<int> untouchedSignals):
    m_untouchedSignals(std::move(untouchedSignals)),
    m_mainPid(getpid()) {
        
    m_origCommandLine.parse(argc, argv);
    m_currentCommandLine = m_origCommandLine;

#if HAVE_SYSTEMD
    if (m_currentCommandLine.daemonType && *m_currentCommandLine.daemonType == DaemonType::Systemd) {
        auto * systemd = dlopen(LIBSYSTEMD_SO, RTLD_NOW | RTLD_LOCAL);
        if (!systemd) {
            WSDLOG_CRITICAL("systemd mode requested but cannot load libsystemd.so: {}", dlerror());
            exit(EXIT_FAILURE);

        }
        m_sdNotify = (decltype(m_sdNotify))dlsym(systemd, "sd_notify");
        if (!m_sdNotify) {
            WSDLOG_CRITICAL("systemd mode requested but cannot find _sd_notify in libsystemd.so: {}", dlerror());
            exit(EXIT_FAILURE);
        }
    }
#endif
}

void AppState::reload() {
    if (m_origCommandLine.configFile) {
        
        m_currentCommandLine = m_origCommandLine;
        m_currentCommandLine.mergeConfigFile(*m_origCommandLine.configFile);
        m_isInitialized ? refresh() : init();
        
    } else if (!m_isInitialized) {

        m_currentCommandLine = m_origCommandLine;
        init();
    }
    
    m_config = Config::make(m_currentCommandLine);
}

void AppState::init() {

    setLogLevel();

    auto devNull = ptl::FileDescriptor::open("/dev/null", O_RDWR);
    redirectStdFile(stdin, devNull);

    if (m_currentCommandLine.daemonType && *m_currentCommandLine.daemonType == DaemonType::Unix) {
        m_savedStdOut = ptl::duplicate(devNull);
        m_savedStdErr = ptl::duplicate(devNull);
        setLogOutput(false); //false to make it replace stdxxx if log file isn't set
        devNull.close();
        daemonize();
    } else {

        m_savedStdOut = ptl::duplicate(stdout);
        m_savedStdErr = ptl::duplicate(stderr);
        setLogOutput(true);
    }

    setPidFile();
    XmlParserInit initXml;
    
    if (getuid() == 0) {
        
        if (!m_currentCommandLine.runAs) {
#if CAN_CREATE_USERS
            WSDLOG_DEBUG("Running as root but no account to run under is specified in configuration. Using {}", WSDDN_DEFAULT_USER_NAME);
            auto pwd = ptl::Passwd::getByName(WSDDN_DEFAULT_USER_NAME);
            if (pwd) {
                m_currentCommandLine.runAs = Identity(pwd->pw_uid, pwd->pw_gid);
            } else  {
                WSDLOG_INFO("User {} does not exist, creating", WSDDN_DEFAULT_USER_NAME);
                m_currentCommandLine.runAs = Identity::createDaemonUser(WSDDN_DEFAULT_USER_NAME);
            }
#else
            WSDLOG_CRITICAL("Running network service as a root is extremely insecure and is not allowed.\n"
                            "Please use one of the following approaches: \n"
                            "  * pass `--user username` command line option to specify which account to run network code under\n"
                            "  * start " WSDDN_PROGNAME " under a non-root account (if using systemd consider DynamicUser approach)"
            );
            exit(EXIT_FAILURE);
#endif
        }
        if (!m_currentCommandLine.chrootDir) {
            WSDLOG_DEBUG("Running as root but no chroot specified in configuration. Using {}", WSDDN_DEFAULT_CHROOT_DIR);
            createMissingDirs(WSDDN_DEFAULT_CHROOT_DIR, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP, Identity::admin());
            m_currentCommandLine.chrootDir = WSDDN_DEFAULT_CHROOT_DIR;
        }
    }

    m_mainPid = getpid();
    m_isInitialized = true;
}

void AppState::refresh() {
    
    if (m_currentCommandLine.logLevel != m_logLevel)
        setLogLevel();
    
    if ( m_currentCommandLine.logFile != m_logFilePath 
#if HAVE_OS_LOG
        || m_logToOsLog != m_currentCommandLine.logToOsLog
#endif
    )
        setLogOutput(false);

    if ( m_currentCommandLine.pidFile != m_pidFilePath)
        setPidFile();
}

void AppState::preFork() {
    spdlog::default_logger()->flush();
    fflush(stdout);
    fflush(stderr);
}

void AppState::postForkInServerProcess() noexcept {
    m_savedStdOut.close();
    m_savedStdErr.close();
    m_pidFile = PidFile();
    
    if (m_currentCommandLine.chrootDir) {
        WSDLOG_DEBUG("Changing root directory");
        ptl::changeRoot(*m_currentCommandLine.chrootDir);
        ptl::changeDirectory("/");
    }
    if (m_currentCommandLine.runAs) {
        WSDLOG_DEBUG("Changing identity");
        m_currentCommandLine.runAs->setMyIdentity();
    }
}

void AppState::notify([[maybe_unused]] DaemonStatus status) {
#if HAVE_SYSTEMD
    if (m_sdNotify) {
        std::string env;
        switch(status) {
            case DaemonStatus::Ready: env += "READY=1"; break;
            case DaemonStatus::Reloading: env += "RELOADING=1"; break;
            case DaemonStatus::Stopping: env += "STOPPING=1"; break;
        }
        env += "\nMAINPID=";
        env += std::to_string(m_mainPid);
        m_sdNotify(0, env.c_str());
    }
#endif
}

void AppState::setLogLevel() {
    if (m_currentCommandLine.logLevel) {
        spdlog::set_level(*m_currentCommandLine.logLevel);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
    m_logLevel = m_currentCommandLine.logLevel;
}

void AppState::setLogOutput(bool firstTime) {

#if HAVE_OS_LOG
    if (m_currentCommandLine.logToOsLog && *m_currentCommandLine.logToOsLog) {
        using Sink = OsLogSink<spdlog::details::null_mutex>;

        auto sink = std::make_shared<Sink>();
        auto logger = std::make_shared<spdlog::logger>("os_log", std::move(sink));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("%v");
    } else 
#endif

    if (m_currentCommandLine.logFile) {
        auto fileFd = openLogFile(*m_currentCommandLine.logFile);
        redirectStdFile(stdout, fileFd);
        redirectStdFile(stderr, fileFd);
        auto logger = spdlog::stdout_logger_st("file");
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P] %L -- %v");
    } else {
        if (!firstTime) {
            redirectStdFile(stdout, m_savedStdOut);
            redirectStdFile(stderr, m_savedStdErr);
        }
        auto logger = isatty(fileno(stdout)) ? spdlog::stdout_color_st("console") : spdlog::stdout_logger_st("console");
        spdlog::set_default_logger(logger);
    #if HAVE_SYSTEMD
        if (m_currentCommandLine.daemonType && *m_currentCommandLine.daemonType == DaemonType::Systemd) {
            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<SystemdLevelFormatter>('l').set_pattern("%l%v");
            spdlog::set_formatter(std::move(formatter));
        } else {
            spdlog::set_pattern("[%l] %v");
        }
    #else
        spdlog::set_pattern("[%l] %v");
    #endif
    }
    m_logFilePath = m_currentCommandLine.logFile;
#if HAVE_OS_LOG
    m_logToOsLog = m_currentCommandLine.logToOsLog;
#endif
}

void AppState::setPidFile() {
    if (m_currentCommandLine.pidFile) {
        auto maybePidFile = PidFile::open(*m_currentCommandLine.pidFile, m_currentCommandLine.runAs);
        if (!maybePidFile)
            throw std::runtime_error("another copy of this application is already running");
        m_pidFile = std::move(*maybePidFile);
    } else {
        m_pidFile = PidFile();
    }
    m_pidFilePath = m_currentCommandLine.pidFile;
}

auto AppState::openLogFile(const std::filesystem::path & filename) -> ptl::FileDescriptor {
        
    std::optional<Identity> owner;
    mode_t mode = S_IRUSR | S_IWUSR;
    mode_t dirMode = mode | S_IXUSR;
    if (getuid() == 0) {
        //make root own it and let "standard" permissions apply
        owner = Identity::admin();
        mode |= S_IRGRP;
        dirMode |= S_IRGRP | S_IXGRP;
    }
    
    createMissingDirs(filename.parent_path(), dirMode, owner);

    auto fd = ptl::FileDescriptor::open(filename, O_WRONLY | O_CREAT | O_APPEND, mode);
    if (owner) {
        auto admin = Identity::admin();
        ptl::changeOwner(fd, admin.uid(), admin.gid());
        ptl::changeMode(fd, S_IRUSR | S_IWUSR | S_IRGRP);
    }
    return fd;
}


void AppState::redirectStdFile(FILE * from, const ptl::FileDescriptor & to) {

    fflush(from);
    duplicateTo(to, from); //yes, reversed! we make the descriptor "get into" FILE
}

//void AppState::closeAllExcept(const int * first, const int * last) {
//
//    std::filesystem::path fdDir = "/proc/self/fd";
//    std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);
//    for (const auto & fdEntry : std::filesystem::directory_iterator(fdDir, ec)) {
//        auto filename = fdEntry.path().filename().native();
//        auto first = filename.c_str();
//        auto last = first + filename.size();
//        unsigned fd = 0;
//        auto res = std::from_chars(first, last, fd, 10);
//        if (res.ec != std::errc() || res.ptr != last)
//            continue;
//        if (std::find(first, last, fd) != last)
//            continue;
//        (void)close(fd);
//    }
//    if (ec) {
//        rlimit lim = {};
//        if (getrlimit(RLIMIT_NOFILE, &lim) == 0) {
//            for(unsigned fd = 3; fd < unsigned(lim.rlim_cur); ++fd) {
//                if (std::find(first, last, fd) != last)
//                    continue;
//                (void)close(fd);
//            }
//        }
//    }
//}

void AppState::daemonize() {

    auto [reportPipeRead, reportPipeWrite] = ptl::Pipe::create();
    char reportBuf[1];
    
//    auto preserve = std::array{
//        fileno(stdin),
//        fileno(stdout),
//        fileno(stderr),
//        reportPipe.first.get(),
//        reportPipe.second.get(),
//        m_savedStdOut.get(),
//        m_savedStdErr.get()
//    };

    fflush(stdout);
    fflush(stderr);

    // 1. Close all open file descriptors except standard input, output, and error
    //    This is only meaningfull if you exec - we don't
    //closeAllExcept(preserve.begin(), preserve.end());
    
    // 2. Reset all signal handlers to their default
    for(int sig = 1; sig < NSIG; ++sig) {
        if (!m_untouchedSignals.contains(sig))
            (void)signal(sig, SIG_DFL); 
    }

    // 3. Reset the signal mask
    auto allSigs = ptl::SignalSet::all();
    for(auto sig: m_untouchedSignals)
        allSigs.del(sig);
    std::error_code ec;
    ptl::setSignalProcessMask(SIG_UNBLOCK, allSigs.get());
    
    // 4. Sanitize the environment block. Eh...

    // 5. Call fork(), to create a background process.

    preFork();
    auto childProcess = ptl::forkProcess();

    if (childProcess) {
        reportPipeWrite.close();
        int ret = EXIT_FAILURE;

        ssize_t res = readFile(reportPipeRead, reportBuf, 1, ec);
        if (res == 1) {
            WSDLOG_INFO("Daemon successfully started");
            ret = EXIT_SUCCESS;
        } else {
            WSDLOG_INFO("Daemon failed to start");
        }

        // 15. Call exit() in the original process.
        exit(ret);
    }

    reportPipeRead.close();

    // 6. In the child, call setsid() to detach from any terminal and create an independent session.

    ptl::setSessionId();
    
    // 7. Call fork() again, to ensure that the daemon can never re-acquire a terminal again.

    preFork();
    childProcess = ptl::forkProcess();

    // 8. Call exit() in the first child, so that only the second child
    //    (the actual daemon process) stays around. This ensures that
    //    the daemon process is re-parented to init/PID 1, as all daemons should be.

    if (childProcess) 
        exit(EXIT_SUCCESS);
    
    // 8a. Start a new process group. This prevents the initial invoker from killing
    //     the daemon if it (or somebody) kills the process group 
    ptl::setProcessGroupId(0, 0);
    
    // 9. Connect /dev/null to standard input, output, and error. ... We are handling output outside of this
    // 10. Reset the umask to 0... We handle it differently

    // 11. Change the current directory to the root directory (/), in order to 
    //     avoid that the daemon involuntarily blocks mount points from being unmounted.

    ptl::changeDirectory("/");
    
    // 12. Write the daemon PID to a PID file... We handle it later
    // 13. Drop privileges, if possible and applicable... We handle it later

    // 14. From the daemon process, notify the original process started that initialization is complete.

    reportBuf[0] = 0;
    writeFile(reportPipeWrite, reportBuf, 1);
}
