// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "sys_util.h"

#if HAVE_OS_LOG

os_log_t OsLogHandle::s_handle = nullptr;
const char * OsLogHandle::s_category = "main";

#endif

#if !HAVE_APPLE_USER_CREATION

    static auto runCreateDaemonUserCommands([[maybe_unused]] const sys_string & name) -> bool {

    #if defined(__linux__) && defined(USERADD_PATH)

        (void)run({USERADD_PATH, "-r", "-d", WSDDN_DEFAULT_CHROOT_DIR, "-s", "/bin/false", name.c_str()});
        return true;

    #elif defined(__linux__) && defined(IS_ALPINE_LINUX) && defined(ADDUSER_PATH) && defined(ADDGROUP_PATH)
    
        //The second addgroup instead of -G for adduser is necessary since for some reason -G doesn't 
        //modify /etc/group when run from here
        (void)run({ADDGROUP_PATH, "-S", name.c_str()});
        (void)run({ADDUSER_PATH, "-S", "-D", "-H", "-h", "/var/empty", "-s", "/sbin/nologin", "-g", name.c_str(), name.c_str()});
        (void)run({ADDGROUP_PATH, name.c_str(), name.c_str()});
        return true;

    #elif (defined(__OpenBSD__) || defined(__NetBSD__)) && defined(USERADD_PATH)

        createMissingDirs(WSDDN_DEFAULT_CHROOT_DIR, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP, Identity::admin());
        (void)run({USERADD_PATH, "-L", "daemon", "-g", "=uid", "-d", WSDDN_DEFAULT_CHROOT_DIR, "-s", "/sbin/nologin", "-c", "WS-Discovery Daemon", name.c_str()});
        return true;

    #elif defined(__HAIKU__) && defined(USERADD_PATH) && defined(GROUPADD_PATH)

        (void)run({GROUPADD_PATH, name.c_str()});
        (void)run({USERADD_PATH, "-g", name.c_str(), "-d", WSDDN_DEFAULT_CHROOT_DIR, "-s", "/bin/false", "-n", "WS-Discovery Daemon", name.c_str()});
        return true;

    #elif defined(__FreeBSD__) && defined(PW_PATH)

        (void)run({PW_PATH, "adduser", name.c_str(), "-d", WSDDN_DEFAULT_CHROOT_DIR, "-s", "/usr/sbin/nologin", "-c", "WS-Discovery Daemon User"});
        return true;

    #elif defined(__sun) && defined(USERADD_PATH) && defined(GROUPADD_PATH)

        (void)run({GROUPADD_PATH, name.c_str()});
        (void)run({USERADD_PATH, "-g", name.c_str(), "-d", WSDDN_DEFAULT_CHROOT_DIR, "-s", "/bin/false", "-c", "WS-Discovery Daemon User", name.c_str()});
        return true;

    #else

        return false;

    #endif
    }

    auto Identity::createDaemonUser(const sys_string & name) -> std::optional<Identity> {

        if (!runCreateDaemonUserCommands(name))
            return {};
        auto pwd = ptl::Passwd::getByName(name);
        if (!pwd)
            throw std::runtime_error(fmt::format("unable to create user {}", name));
        return Identity(pwd->pw_uid, pwd->pw_gid);
    }

#endif

int run(const ptl::StringRefArray & args) {
    ptl::SpawnAttr spawnAttr;
#ifndef __HAIKU__
    spawnAttr.setFlags(POSIX_SPAWN_SETSIGDEF);
    auto sigs = ptl::SignalSet::all();
    sigs.del(SIGKILL);
    sigs.del(SIGSTOP);
    spawnAttr.setSigDefault(sigs);
#endif
    
    auto proc = spawn(args, ptl::SpawnSettings().attr(spawnAttr).usePath());
    
    auto stat = proc.wait().value();
    if (WIFEXITED(stat))
        return WEXITSTATUS(stat);
    if (WIFSIGNALED(stat))
        return 128+WTERMSIG(stat);
    throw std::runtime_error(fmt::format("`{} finished with status 0x{:X}`", args, stat));
}

void shell(const ptl::StringRefArray & args, bool suppressStdErr, std::function<void (const ptl::FileDescriptor & fd)> reader) {
    auto [read, write] = ptl::Pipe::create();
    ptl::SpawnAttr spawnAttr;
#ifndef __HAIKU__
    spawnAttr.setFlags(POSIX_SPAWN_SETSIGDEF);
    auto sigs = ptl::SignalSet::all();
    sigs.del(SIGKILL);
    sigs.del(SIGSTOP);
    spawnAttr.setSigDefault(sigs);
#endif
    
    ptl::SpawnFileActions act;
    act.addDuplicateTo(write, stdout);
    act.addClose(read);
    if (suppressStdErr) {
       act.addOpen(stderr, "/dev/null", O_WRONLY, 0);
    }
    auto proc = spawn(args, ptl::SpawnSettings().attr(spawnAttr).fileActions(act).usePath());
    write.close();

    reader(read);

    auto stat = proc.wait().value();
    if (WIFEXITED(stat)) {
        auto res = WEXITSTATUS(stat);
        if (res == 0)
            return;

        throw std::runtime_error(fmt::format("`{} exited with code {}`", args, res));
    }
    if (WIFSIGNALED(stat)) {
        throw std::runtime_error(fmt::format("`{} exited due to signal {}`", args, WTERMSIG(stat)));
    }
    throw std::runtime_error(fmt::format("`{} finished with status 0x{:X}`", args, stat));
}
