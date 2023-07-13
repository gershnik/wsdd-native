// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "app_state.h"
#include "server_manager.h"
#include "exc_handling.h"

#define EXIT_RELOAD 2

static_assert(EXIT_RELOAD != EXIT_FAILURE);


static std::random_device g_RandomDevice;
std::mt19937 g_Random(g_RandomDevice());

static std::optional<ptl::ChildProcess> g_maybeChildProcess;
static std::atomic<sig_atomic_t> g_reload = 0;
static ptl::SignalSet g_controlSignals;


static inline void blockSignals() {
    ptl::setSignalProcessMask(SIG_BLOCK, g_controlSignals);
}
static inline void unblockSignals() {
    ptl::setSignalProcessMask(SIG_UNBLOCK, g_controlSignals);
}

static auto waitForChild() -> std::optional<int> {
    
    WSDLOG_INFO("Waiting for child");
    
    auto oldSigInt = ptl::setSignalHandler(SIGINT, [](int) {
        assert(g_maybeChildProcess);
        (void)::kill(g_maybeChildProcess->get(), SIGINT);
    });
    auto oldSigTerm = ptl::setSignalHandler(SIGTERM, [](int) {
        assert(g_maybeChildProcess);
        (void)::kill(g_maybeChildProcess->get(), SIGTERM);
    });
    auto oldSigHup = ptl::setSignalHandler(SIGHUP, [](int) {
        g_reload = 1;
        assert(g_maybeChildProcess);
        (void)::kill(g_maybeChildProcess->get(), SIGINT);
    });
    
    unblockSignals();
    
    int status = 0;
    for ( ; ; ) {
        ErrorWhitelist ec(EINTR);
        auto maybeStatus = g_maybeChildProcess->wait(ec);
        if (maybeStatus) {
            status = *maybeStatus;
            break;
        }
    }
    ptl::setSignalHandler(SIGINT, oldSigInt);
    ptl::setSignalHandler(SIGINT, oldSigTerm);
    ptl::setSignalHandler(SIGINT, oldSigHup);
    
    assert(!*g_maybeChildProcess);
    g_maybeChildProcess = std::nullopt;
    
    if (WIFSIGNALED(status)) { //child exited by signal
        int termsig = WTERMSIG(status);
        if (termsig == SIGINT) {
            WSDLOG_INFO("Child killed by SIGINT");
            return std::nullopt;
        }
        WSDLOG_INFO("Child killed by signal {} - exiting", ptl::signalName(termsig));
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


static void serve(const refcnt_ptr<Config> & config, ptl::FileDescriptor * monitorDesc) {
    
    static char dummyBuffer[1];
    
    WSDLOG_INFO("Starting processing");
    
    asio::io_context ctxt;
    
    ServerManager serverManager(ctxt, config, createInterfaceMonitor, createHttpServer, createUdpServer);
    
    std::shared_ptr<asio::readable_pipe> monitorPipe;
    asio::signal_set signals(ctxt, SIGINT, SIGTERM, SIGHUP);
    
    signals.async_wait([&](const asio::error_code & ec, int signo){
        if (ec)
            throw std::system_error(ec, "async waiting for signal failed");
        WSDLOG_INFO("Received signal: {}", ptl::signalName(signo));
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
            
            ptl::Pipe monitorPipe;
            
            blockSignals();
            
            appState.reload();
            g_reload = 0;
            
            if (appState.shouldFork()) {
                
                WSDLOG_INFO("Starting child");
                
                monitorPipe = ptl::Pipe::create();
                
                appState.preFork();
                
                g_maybeChildProcess = ptl::forkProcess();
            }
            
            if (!g_maybeChildProcess) { //standalone
                
                appState.notify(AppState::DaemonStatus::Ready);
                serve(appState.config(), nullptr);
                
                if (!g_reload) {
                    appState.notify(AppState::DaemonStatus::Stopping);
                    return EXIT_SUCCESS;
                }
                
            } else if (!*g_maybeChildProcess) { //child

            #if HAVE_OS_LOG
                OsLogHandle::resetInChild();
            #endif
                
                WSDLOG_INFO("Child started");
                
                appState.postForkInServerProcess();
                
                monitorPipe.writeEnd.close();
                                
                serve(appState.config(), &monitorPipe.readEnd);
                
                return g_reload ? EXIT_RELOAD : EXIT_SUCCESS;
                
            } else { //parent
                
                monitorPipe.readEnd.close();
                
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
        g_controlSignals.add(SIGINT);
        g_controlSignals.add(SIGTERM);
        g_controlSignals.add(SIGHUP);
        blockSignals();
        
        //set default handlers
        ptl::setSignalHandler(SIGINT, [](int) {
            exit(EXIT_SUCCESS);
        });
        ptl::setSignalHandler(SIGTERM, [](int) {
            exit(EXIT_SUCCESS);
        });
        ptl::setSignalHandler(SIGHUP, SIG_IGN);
                
        umask(S_IRWXG | S_IRWXO);
        
        AppState appState(argc, argv, {SIGINT, SIGTERM, SIGHUP});
        
        return runServer(appState);
        
    } catch (std::exception & ex) {
        fmt::print(stderr, "Exception: {}\n", ex.what());
        fmt::print(stderr, "{}", formatCaughtExceptionBacktrace());
    }
    return EXIT_FAILURE;
}
