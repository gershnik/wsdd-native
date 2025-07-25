// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "sys_util.h"

#if HAVE_OS_LOG

os_log_t OsLogHandle::s_handle = nullptr;
const char * OsLogHandle::s_category = "main";

#endif

#if HAVE_USERADD || HAVE_PW

auto Identity::createDaemonUser(const sys_string & name) -> Identity {

#if HAVE_USERADD
    sys_string command = S("useradd -r -d " WSDDN_DEFAULT_CHROOT_DIR " -s /bin/false '") + name + S("'");
#elif HAVE_PW
    sys_string command = S("pw adduser '") + name + S("' -d " WSDDN_DEFAULT_CHROOT_DIR " -s /bin/false -c \"WS-Discovery Daemon User\"");
#endif
    (void)!system(command.c_str());
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
