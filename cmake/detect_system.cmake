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
check_struct_has_member("struct tm" "tm_gmtoff" "time.h" HAVE_TM_TM_GMTOFF)


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

if (NOT DEFINED USERADD_PATH)
    find_program(USERADD_PATH useradd PATHS /usr/sbin /bin NO_DEFAULT_PATH)
    if (USERADD_PATH)
        message(STATUS "Looking for useradd - found at ${USERADD_PATH}")
    else()
        message(STATUS "Looking for useradd - not found")
    endif()
endif()

if (NOT DEFINED GROUPADD_PATH)
    find_program(GROUPADD_PATH groupadd PATHS /usr/sbin /bin NO_DEFAULT_PATH)
    if (GROUPADD_PATH)
        message(STATUS "Looking for groupadd - found at ${GROUPADD_PATH}")
    else()
        message(STATUS "Looking for groupadd - not found")
    endif()
endif()

if (NOT DEFINED PW_PATH)
    find_program(PW_PATH pw PATHS /usr/sbin NO_DEFAULT_PATH)
    if (PW_PATH)
        message(STATUS "Looking for pw - found at ${PW_PATH}")
    else()
        message(STATUS "Looking for pw - not found")
    endif()
endif()

# Alpine Linux needs special treatment

if (NOT DEFINED IS_ALPINE_LINUX)
    if(EXISTS "/etc/os-release")
        file(STRINGS "/etc/os-release" OS_RELEASE_CONTENTS)
        foreach(line IN LISTS OS_RELEASE_CONTENTS)
            if(line MATCHES "^ID=alpine$")
                set(IS_ALPINE_LINUX TRUE CACHE INTERNAL "whether this is Alpine Linux")
                message(STATUS "This is Alpine Linux")
                break()
            endif()
        endforeach()
    endif()
    if (NOT DEFINED IS_ALPINE_LINUX)
        set(IS_ALPINE_LINUX FALSE CACHE INTERNAL "whether this is Alpine Linux")
    endif()
endif()

if(IS_ALPINE_LINUX)
    if (NOT DEFINED ADDUSER_PATH)
        find_program(ADDUSER_PATH adduser PATHS /usr/sbin NO_DEFAULT_PATH)
        if (ADDUSER_PATH)
            message(STATUS "Looking for adduser - found at ${ADDUSER_PATH}")
        else()
            message(STATUS "Looking for adduser - not found")
        endif()
    endif()

    if (NOT DEFINED ADDGROUP_PATH)
        find_program(ADDGROUP_PATH addgroup PATHS /usr/sbin NO_DEFAULT_PATH)
        if (ADDGROUP_PATH)
            message(STATUS "Looking for addgroup - found at ${ADDGROUP_PATH}")
        else()
            message(STATUS "Looking for addgroup - not found")
        endif()
    endif()

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


find_program(ASCIIDOCTOR_PATH asciidoctor)
