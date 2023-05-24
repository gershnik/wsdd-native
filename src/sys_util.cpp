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
    auto pwd = Passwd::getByName(name);
    if (!pwd)
        throw std::runtime_error(fmt::format("unable to create user {}", name));
    return Identity(pwd->pw_uid, pwd->pw_gid);
}

#endif