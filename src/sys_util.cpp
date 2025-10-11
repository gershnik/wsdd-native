// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "sys_util.h"

#if HAVE_OS_LOG

os_log_t OsLogHandle::s_handle = nullptr;
const char * OsLogHandle::s_category = "main";

#endif

#if !HAVE_APPLE_USER_CREATION

    static auto runCreateDaemonUserCommands(const sys_string & name) -> bool {
        
    #if defined(__linux__) && defined(USERADD_PATH)

        sys_string command = S(USERADD_PATH " -r -d " WSDDN_DEFAULT_CHROOT_DIR " -s /bin/false '") + name + S("'");
        (void)!system(command.c_str());
        return true;

    #elif (defined(__OpenBSD__) || defined(__NetBSD__)) && defined(USERADD_PATH)

        createMissingDirs(WSDDN_DEFAULT_CHROOT_DIR, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP, Identity::admin());
        sys_string command = S(USERADD_PATH " -L daemon -g =uid -d " WSDDN_DEFAULT_CHROOT_DIR " -s /sbin/nologin -c \"WS-Discovery Daemon\" '") + name + S("'");
        (void)!system(command.c_str());
        return true;

    #elif defined(__HAIKU__) && defined(USERADD_PATH) && defined(GROUPADD_PATH)

        createMissingDirs(WSDDN_DEFAULT_CHROOT_DIR, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP, Identity::admin());

        sys_string command = S(GROUPADD_PATH " '") + name + S("'");
        (void)!system(command.c_str());
        command = S(USERADD_PATH " -g ") + name + S(" -d " WSDDN_DEFAULT_CHROOT_DIR " -s /bin/false -n \"WS-Discovery Daemon\" '") + name + S("'");
        (void)!system(command.c_str());
        return true;

    #elif defined(__FreeBSD__) && defined(PW_PATH)

        sys_string command = S(PW_PATH " adduser '") + name + S("' -d " WSDDN_DEFAULT_CHROOT_DIR " -s /bin/false -c \"WS-Discovery Daemon User\"");
        (void)!system(command.c_str());
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

void shell(const ptl::StringRefArray & args, bool suppressStdErr, std::function<void (const ptl::FileDescriptor & fd)> reader) {
    auto [read, write] = ptl::Pipe::create();
    ptl::SpawnAttr spawnAttr;
    spawnAttr.setFlags(POSIX_SPAWN_SETSIGDEF);
    auto sigs = ptl::SignalSet::all();
    sigs.del(SIGKILL);
    sigs.del(SIGSTOP);
    spawnAttr.setSigDefault(sigs);
    
    ptl::SpawnFileActions act;
    act.addDuplicateTo(write, stdout);
    act.addClose(read);
    if (suppressStdErr) {
       act.addOpen(stderr, "/dev/null", O_WRONLY, 0);
    }
    auto proc = spawn(args, ptl::SpawnSettings().fileActions(act).usePath());
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
