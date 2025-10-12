# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(CheckCXXSourceCompiles)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(CMakePushCheckState)

check_cxx_source_compiles("
    #include <linux/netlink.h>
    #include <linux/rtnetlink.h>
    int main() {}" 
HAVE_NETLINK)

if (NOT HAVE_NETLINK)

    check_cxx_source_compiles("
        #include <net/if.h>
        #include <net/if_dl.h>
        #include <net/route.h>
        #include <sys/socket.h>
        int main() { 
            int x = PF_ROUTE;
            size_t s = sizeof(rt_msghdr);
        }" 
    HAVE_PF_ROUTE)

    check_cxx_source_compiles("
        #include <net/if.h>
        #include <net/if_dl.h>
        #include <sys/socket.h>
        #include <sys/sysctl.h>
        int main() { 
            int x = NET_RT_IFLIST; 
        }" 
    HAVE_SYSCTL_PF_ROUTE)

    check_cxx_source_compiles("
        #include <sys/socket.h>
        #include <sys/sockio.h>
        #include <netinet/in.h>
        #include <net/if.h>
        int main() { 
            int x = SIOCGLIFCONF; 
            lifconf conf{};
        }" 
    HAVE_SIOCGLIFCONF)

    check_cxx_source_compiles("
        #include <sys/socket.h>
        #include <sys/sockio.h>
        #include <netinet/in.h>
        #include <net/if.h>
        int main() {
            int x = SIOCGIFCONF;
            ifconf conf{};
        }"
    HAVE_SIOCGIFCONF)

endif()

check_struct_has_member("struct sockaddr" "sa_len" "sys/types.h;sys/socket.h" HAVE_SOCKADDR_SA_LEN)

check_include_files(execinfo.h HAVE_EXECINFO_H)
check_library_exists(execinfo backtrace "" HAVE_EXECINFO_LIB)

check_cxx_source_compiles("
    #include <cxxabi.h>
    int main() { 
        int stat;
        abi::__cxa_demangle(\"abc\", 0, 0, &stat); 
    }"
HAVE_CXXABI_H)


check_cxx_source_compiles("
    #include <cxxabi.h>
    int main() { 
        using x = decltype(abi::__cxa_throw);
    }"
HAVE_ABI_CXA_THROW)


find_program(USERADD_PATH useradd PATHS /usr/sbin NO_DEFAULT_PATH)
if (USERADD_PATH)
    set(HAVE_USERADD ON CACHE INTERNAL "Whether platform has useradd command")
else()
    set(HAVE_USERADD OFF CACHE INTERNAL "Whether platform has useradd command")
endif()

find_program(PW_PATH pw PATHS /usr/sbin NO_DEFAULT_PATH)
if (PW_PATH)
    set(HAVE_PW ON CACHE INTERNAL "Whether platform has pw command")
else()
    set(HAVE_PW OFF CACHE INTERNAL "Whether platform has pw command")
endif()

if (WSDDN_WITH_SYSTEMD STREQUAL "yes" OR WSDDN_WITH_SYSTEMD STREQUAL "auto" AND NOT DEFINED CACHE{HAVE_SYSTEMD})

    message(CHECK_START "Looking for systemd")

    find_library(LIBSYSTEMD_LIBRARY NAMES systemd systemd-daemon)

    if (LIBSYSTEMD_LIBRARY)
        if(IS_SYMLINK "${LIBSYSTEMD_LIBRARY}")
            file(READ_SYMLINK "${LIBSYSTEMD_LIBRARY}" LIBSYSTEMD_SO)
            if(NOT IS_ABSOLUTE "${LIBSYSTEMD_SO}")
                get_filename_component(dir "${LIBSYSTEMD_LIBRARY}" DIRECTORY)
                set(LIBSYSTEMD_SO "${dir}/${LIBSYSTEMD_SO}")
            endif()
        else()
            set(LIBSYSTEMD_SO "${LIBSYSTEMD_LIBRARY}")
        endif()

        set(LIBSYSTEMD_SO "${LIBSYSTEMD_SO}"  CACHE INTERNAL "" FORCE)
    endif()

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES ${LIBSYSTEMD_LIBRARY})
    check_include_files(systemd/sd-daemon.h HAVE_SYSTEMD_SD_DAEMON_H)
    check_function_exists(sd_notify HAVE_SYSTEMD_SD_NOTIFY)
    cmake_pop_check_state()

    if (HAVE_SYSTEMD_SD_DAEMON_H AND HAVE_SYSTEMD_SD_NOTIFY AND LIBSYSTEMD_SO)

        set(HAVE_SYSTEMD ON CACHE INTERNAL "")
        message(CHECK_PASS "found in ${LIBSYSTEMD_SO}")

    else()

        message(CHECK_FAIL "not found")
        if (WSDDN_WITH_SYSTEMD STREQUAL "yes")
            message(FATAL_ERROR "systemd is required but not found")
        endif()

    endif()

endif()


find_program(PANDOC_PATH pandoc)
find_program(GROFF_PATH groff)
