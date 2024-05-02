# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(CheckCXXSourceCompiles)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)

check_cxx_source_compiles("
    #include <linux/netlink.h>
    #include <linux/rtnetlink.h>
    int main() {}" 
HAVE_NETLINK)

if (NOT HAVE_NETLINK)

    check_cxx_source_compiles("
        #include <net/if.h>
        #include <net/if_dl.h>
        #include <sys/socket.h>
        int main() { 
            int x = PF_ROUTE;
        }" 
    HAVE_PF_ROUTE)

    check_cxx_source_compiles("
        #include <sys/sysctl.h>
        #include <net/if.h>
        #include <net/if_dl.h>
        #include <sys/socket.h>
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

endif()

check_cxx_source_compiles("
    #include <sys/socket.h>
    int main() {
        struct sockaddr addr = {};
        size_t x = addr.sa_len;
    }" 
HAVE_SOCKADDR_SA_LEN)

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

    find_package(PkgConfig)

    find_library(LIBSYSTEMD_LIBRARY NAMES systemd systemd-daemon)

    if (NOT LIBSYSTEMD_LIBRARY)
        set(LIBSYSTEMD_LIBRARY "" CACHE INTERNAL "" FORCE)
    endif()

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES ${LIBSYSTEMD_LIBRARY})
    check_include_files(systemd/sd-daemon.h HAVE_SYSTEMD_SD_DAEMON_H)
    check_function_exists(sd_notify HAVE_SYSTEMD_SD_NOTIFY)
    cmake_pop_check_state()

    if (HAVE_SYSTEMD_SD_DAEMON_H AND HAVE_SYSTEMD_SD_NOTIFY)

        set(HAVE_SYSTEMD ON CACHE INTERNAL "")
        message(CHECK_PASS "found in ${LIBSYSTEMD_LIBRARY}")

    else()

        message(CHECK_FAIL "not found")
        if (WSDDN_WITH_SYSTEMD STREQUAL "yes")
            message(FATAL_ERROR "systemd is required but not found")
        endif()

    endif()

endif()

if (HAVE_SYSTEMD AND NOT DEFINED CACHE{SYSTEMD_SYSTEM_UNIT_DIR}) 

    message(CHECK_START "Looking for systemd system unit dir")

    find_package(PkgConfig)

    if (PKG_CONFIG_FOUND)
        execute_process(
            COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd
            OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR
            RESULT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR_RESULT)

        if (${SYSTEMD_SYSTEM_UNIT_DIR_RESULT} EQUAL 0)
        string(STRIP ${SYSTEMD_SYSTEM_UNIT_DIR} SYSTEMD_SYSTEM_UNIT_DIR)
            set(SYSTEMD_SYSTEM_UNIT_DIR ${SYSTEMD_SYSTEM_UNIT_DIR} CACHE STRING "systemd unit dir")
        endif()
    endif()

    if (SYSTEMD_SYSTEM_UNIT_DIR)
        message(CHECK_PASS "Found in ${SYSTEMD_SYSTEM_UNIT_DIR}")
    else()
        message(CHECK_PASS "Not found, assuming /usr/lib/systemd/system")
        set(SYSTEMD_SYSTEM_UNIT_DIR "/usr/lib/systemd/system" CACHE STRING "systemd unit dir")
    endif()

endif()

find_program(PANDOC_PATH pandoc)
find_program(GROFF_PATH groff)
