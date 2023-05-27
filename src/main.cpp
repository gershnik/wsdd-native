// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "app_state.h"
#include "server_manager.h"
#include "exc_handling.h"

#define EXIT_RELOAD 2

static_assert(EXIT_RELOAD != EXIT_FAILURE);


static std::random_device g_RandomDevice;
std::mt19937 g_Random(g_RandomDevice());

static pid_t g_childPid = -1;
static std::atomic<sig_atomic_t> g_reload = 0;
static sigset_t g_controlSignals;


static void blockSignals() {
    if (sigprocmask(SIG_BLOCK, &g_controlSignals, nullptr) != 0)
        throwErrno("sigprocmask(SIG_BLOCK)", errno);
}

static void unblockSignals() {
    if (sigprocmask(SIG_UNBLOCK, &g_controlSignals, nullptr) != 0)
        throwErrno("sigprocmask(SIG_UNBLOCK)", errno);
}

static auto waitForChild() -> std::optional<int> {
    
    WSDLOG_INFO("Waiting for child");
    
    auto oldSigInt = setSignalHandler(SIGINT, [](int) {
        kill(g_childPid, SIGINT);
    });
    auto oldSigTerm = setSignalHandler(SIGTERM, [](int) {
        kill(g_childPid, SIGTERM);
    });
    auto oldSigHup = setSignalHandler(SIGHUP, [](int) {
        g_reload = 1;
        kill(g_childPid, SIGINT);
    });
    
    unblockSignals();
    
    int status = 0;
    pid_t waitRet;
    for ( ; ; ) {
        waitRet = waitpid(g_childPid, &status, 0);
        if (waitRet >= 0)
            break;
        auto err = errno;
        if (err != EINTR)
            throwErrno("waitpid()", errno);
    }
    setSignalHandler(SIGINT, oldSigInt);
    setSignalHandler(SIGINT, oldSigTerm);
    setSignalHandler(SIGINT, oldSigHup);
    
    g_childPid = -1;
    
    if (WIFSIGNALED(status)) { //child exited by signal
        int termsig = WTERMSIG(status);
        if (termsig == SIGINT) {
            WSDLOG_INFO("Child killed by SIGINT");
            return std::nullopt;
        }
        WSDLOG_INFO("Child killed by signal {} - exiting", signalName(termsig));
        return EXIT_FAILURE;
    }
    
    if (int exitCode = WEXITSTATUS(status); exitCode != 0)  { //child exited with failure
        if (exitCode == EXIT_RELOAD) {
            WSDLOG_INFO("Child exited with EXIT_RELOAD");
            g_reload = 1;
            return std::nullopt;
        }
        WSDLOG_INFO("Child exited with code {} - exiting", exitCode);
        return exitCode;
    }
    
    WSDLOG_INFO("Child exited successfully");
    return std::nullopt;
}


static void serve(const refcnt_ptr<Config> & config, FileDescriptor * monitorDesc) {
    
    static char dummyBuffer[1];
    
    WSDLOG_INFO("Starting processing");
    
    asio::io_context ctxt;
    
    ServerManager serverManager(ctxt, config, createInterfaceMonitor, createHttpServer, createUdpServer);
    
    std::shared_ptr<asio::readable_pipe> monitorPipe;
    asio::signal_set signals(ctxt, SIGINT, SIGTERM, SIGHUP);
    
    signals.async_wait([&](const asio::error_code & ec, int signo){
        if (ec)
            throw std::system_error(ec);
        WSDLOG_INFO("Received signal: {}", signalName(signo));
        serverManager.stop(true);
        if (monitorPipe)
            monitorPipe.reset();
        if (signo == SIGHUP)
            g_reload = 1;
    });
    unblockSignals();
    
    
    if (monitorDesc) {
        monitorPipe = std::make_shared<asio::readable_pipe>(ctxt, monitorDesc->get());
        monitorDesc->detach();
        monitorPipe->async_read_some(asio::buffer(dummyBuffer), [&](asio::error_code /*ec*/, size_t /*bytesRead*/) {
            if (!monitorPipe)
                return;
            WSDLOG_INFO("Parent process exited");
            kill(getpid(), SIGINT);
        });
    }
    
    serverManager.start();
    
    ctxt.run();
    
    WSDLOG_INFO("Stopped processing");
}

auto runServer(AppState & appState) -> int {
    
    try {
        for ( ; ; ) {
            
            std::pair<FileDescriptor, FileDescriptor> monitorPipe;
            
            blockSignals();
            
            appState.reload();
            g_reload = 0;
            
            if (appState.shouldFork()) {
                
                WSDLOG_INFO("Starting child");
                
                monitorPipe = FileDescriptor::pipe().value();
                
                appState.preFork();
                
                g_childPid = forkProcess();
            }
            
            if (g_childPid < 0) { //standalone
                
                appState.notify(AppState::DaemonStatus::Ready);
                serve(appState.config(), nullptr);
                
                if (!g_reload) {
                    appState.notify(AppState::DaemonStatus::Stopping);
                    return EXIT_SUCCESS;
                }
                
            } else if (g_childPid == 0) { //child

            #if HAVE_OS_LOG
                OsLogHandle::resetInChild();
            #endif
                
                WSDLOG_INFO("Child started");
                
                appState.postForkInServerProcess();
                
                monitorPipe.second = FileDescriptor();
                                
                serve(appState.config(), &monitorPipe.first);
                
                return g_reload ? EXIT_RELOAD : EXIT_SUCCESS;
                
            } else { //parent
                
                monitorPipe.first = FileDescriptor();
                
                appState.notify(AppState::DaemonStatus::Ready);
                if (auto res = waitForChild())
                    return *res;
                
                if (!g_reload) {
                    appState.notify(AppState::DaemonStatus::Stopping);
                    return EXIT_SUCCESS;
                }
            }
            
            appState.notify(AppState::DaemonStatus::Reloading);
            WSDLOG_INFO("Reloading configuration");
        }
        
    } catch (std::exception & ex) {
        WSDLOG_ERROR("Exception: {}", ex.what());
        WSDLOG_ERROR("{}", formatCaughtExceptionBacktrace());
    }
    return EXIT_FAILURE;
}


int main(int argc, char * argv[]) {
    
    try {
        sigaddset(&g_controlSignals, SIGINT);
        sigaddset(&g_controlSignals, SIGTERM);
        sigaddset(&g_controlSignals, SIGHUP);
        blockSignals();
        
        //set default handlers
        setSignalHandler(SIGINT, [](int) {
            exit(EXIT_SUCCESS);
        });
        setSignalHandler(SIGTERM, [](int) {
            exit(EXIT_SUCCESS);
        });
        setSignalHandler(SIGHUP, SIG_IGN);
                
        umask(S_IRWXG | S_IRWXO);
        
        AppState appState(argc, argv, {SIGINT, SIGTERM, SIGHUP});
        
        return runServer(appState);
        
    } catch (std::exception & ex) {
        fmt::print(stderr, "Exception: {}\n", ex.what());
        fmt::print(stderr, "{}", formatCaughtExceptionBacktrace());
    }
    return EXIT_FAILURE;
}
