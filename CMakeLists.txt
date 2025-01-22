# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.23)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) #set the timestamps of all extracted contents to the time of the extraction
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(BUILD_SHARED_LIBS OFF)

set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY ON)

if (NOT WSDDN_VERSION)

    file(READ VERSION WSDDN_VERSION)
    if (NOT WSDDN_VERSION)
        message(FATAL_ERROR "WSDDN_VERSION not set and VERSION file not present. Cannot determine version")
    endif()
    string(STRIP ${WSDDN_VERSION} WSDDN_VERSION)

endif()

message(NOTICE WSDDN_VERSION: ${WSDDN_VERSION})

project(wsddn)

#By default prefer system libraries on Mac and source compiled on other platforms
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(WSDDN_PREFER_SYSTEM_DEFAULT ON)
else()
    set(WSDDN_PREFER_SYSTEM_DEFAULT OFF)
endif()

option(WSDDN_PREFER_SYSTEM "whether to prefer system versions of dependencies (such as libxm2)" ${WSDDN_PREFER_SYSTEM_DEFAULT})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(WSDDN_WITH_SYSTEMD "auto" CACHE STRING "enable systemd scripts and notification support")
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(WSDDN_BUNDLE_IDENTIFIER "io.github.gershnik.wsddn" CACHE STRING "macOS bundle identifier")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(CheckIPOSupported)

check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
endif()

include(cmake/dependencies.cmake)
include(cmake/detect_system.cmake)

if (NOT HAVE_NETLINK AND NOT HAVE_PF_ROUTE)
    message(FATAL_ERROR "Neither netlink nor PF_ROUTE headers were detected on your machine.\nThis platform seems not supported.")
endif()

if (HAVE_PF_ROUTE AND NOT HAVE_SYSCTL_PF_ROUTE AND NOT HAVE_SIOCGLIFCONF)
    message(FATAL_ERROR "Neither BSD sysctl(CTL_NET, PF_ROUTE) not ioctl(SIOCGLIFCONF) were detected on your machine.\nThis platform seems not supported.")
endif()

add_executable(wsddn)

set_target_properties(argum PROPERTIES FOLDER "Dependencies")
set_target_properties(sys_string PROPERTIES FOLDER "Dependencies")
set_target_properties(isptr PROPERTIES FOLDER "Dependencies")
set_target_properties(ptl PROPERTIES FOLDER "Dependencies")
if (TARGET LibXml2)
    set_target_properties(LibXml2 PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET modern-uuid-static)
    set_target_properties(modern-uuid-static PROPERTIES FOLDER "Dependencies")
endif()
set_target_properties(spdlog PROPERTIES FOLDER "Dependencies")
set_target_properties(fmt PROPERTIES FOLDER "Dependencies")


set_target_properties(wsddn PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    C_STANDARD 11
    CXX_STANDARD_REQUIRED True
    C_STANDARD_REQUIRED True
)

target_link_libraries(wsddn
PRIVATE
    argum
    sys_string
    intrusive-shared-ptr
    ptl::ptl
    LibXml2::LibXml2
    modern-uuid::modern-uuid-static
    fmt::fmt
    spdlog::spdlog
    tomlplusplus::tomlplusplus

    "$<$<PLATFORM_ID:Darwin>:-framework SystemConfiguration>"
    "$<$<PLATFORM_ID:Darwin>:-framework OpenDirectory>"

    "$<$<OR:$<PLATFORM_ID:Linux>,$<BOOL:${HAVE_SYSTEMD}>>:dl>"

    "$<$<PLATFORM_ID:SunOS>:socket>"
    
    "$<$<BOOL:${HAVE_EXECINFO_LIB}>:execinfo>"
)

target_compile_options(wsddn 
PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-pedantic;-ftemplate-backtrace-limit=0>
    $<$<CXX_COMPILER_ID:AppleClang>:-Wall;-Wextra;-pedantic;-fpch-instantiate-templates;-ftemplate-backtrace-limit=0>
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-pedantic>   
)

target_link_options(wsddn 
PRIVATE
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-object_path_lto,${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lto.o>"
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/LTOCache>"
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-no_adhoc_codesign>"
    "$<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,13.5>>:-Wl,-reproducible>"
    "$<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15.0>>:LINKER:-no_warn_duplicate_libraries>" #see https://gitlab.kitware.com/cmake/cmake/-/issues/25297
)

target_compile_definitions(wsddn
PRIVATE
    SYS_STRING_USE_GENERIC=1
    "$<$<PLATFORM_ID:SunOS>:ASIO_DISABLE_DEV_POLL=1>"
)

target_include_directories(wsddn
PRIVATE
    ${CMAKE_BINARY_DIR}
    ${asio_SOURCE_DIR}/include
    ${outcome_SOURCE_DIR}/single-header
)

target_precompile_headers(wsddn
PRIVATE 
    src/pch.h
)

set(UTIL_SOURCES
    src/util.h
    src/sys_socket.h
    src/sys_util.h
    src/sys_util.cpp
    src/sys_util_mac.cpp
    src/xml_wrapper.h
    src/pid_file.h
)
source_group("Utils" FILES ${UTIL_SOURCES})

set(SERVERS_SOURCES
    src/interface_monitor.h
    src/interface_monitor_bsd.cpp
    src/interface_monitor_linux.cpp
    src/http_request_parser.h
    src/http_request_parser.cpp
    src/http_request.h
    src/http_request.cpp
    src/http_response.h
    src/http_response.cpp
    src/http_server.h
    src/http_server.cpp
    src/udp_server.h
    src/udp_server.cpp
    src/wsd_server.h
    src/wsd_server.cpp
    src/server_manager.h
    src/server_manager.cpp
)
source_group("Servers" FILES ${SERVERS_SOURCES})

set(MAIN_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/sys_config.h
    src/pch.h

    src/app_state.h
    src/app_state.cpp
    src/config.h
    src/config.cpp
    src/config_mac.cpp
    src/config_samba.cpp
    src/command_line.h
    src/command_line.cpp
    src/exc_handling.h
    src/exc_handling.cpp
    src/main.cpp
)
source_group("Main" FILES ${MAIN_SOURCES})

set(CMAKE_SOURCES
    cmake/dependencies.cmake
    cmake/detect_system.cmake
    cmake/install.cmake
    src/sys_config.h.in
)
source_group("CMake" FILES ${CMAKE_SOURCES})

target_sources(wsddn
PRIVATE
    ${UTIL_SOURCES}
    ${SERVERS_SOURCES}
    ${MAIN_SOURCES}
    ${CMAKE_SOURCES}
)

file(READ src/sys_config.h.in CONFIG_CONTENT)
string(CONFIGURE ${CONFIG_CONTENT} CONFIG_CONTENT)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sys_config.h CONTENT ${CONFIG_CONTENT})


if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    configure_file(src/Info.template.plist Info.plist @ONLY)

    target_link_options(wsddn 
    PRIVATE 
        LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    )

    set_target_properties(wsddn PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
    
    if (CMAKE_GENERATOR MATCHES "Xcode")
        set_target_properties(wsddn PROPERTIES
            XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
            XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
            XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS YES
            XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym
        )
    else()
        add_custom_command(TARGET wsddn POST_BUILD
            COMMAND dsymutil "$<TARGET_FILE:wsddn>" -o "$<TARGET_FILE:wsddn>.dSYM"
            COMMAND codesign --force --sign - --timestamp=none --options runtime "$<TARGET_FILE:wsddn>" 
        )
    endif()
endif()

add_custom_target(manpage ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/wsddn.8.gz ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8.html
    SOURCES doc/wsddn.8.md
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/wsddn.8.gz
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/doc
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8 ${CMAKE_CURRENT_BINARY_DIR}/doc/
    COMMAND gzip -f ${CMAKE_CURRENT_BINARY_DIR}/doc/wsddn.8
    VERBATIM
)

if (PANDOC_PATH)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8.md
        COMMAND ${PANDOC_PATH} --standalone --to man ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8.md -o ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8
        VERBATIM
    )

endif()

if (GROFF_PATH)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8.html
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8
        COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8 | ${GROFF_PATH} -mandoc -Thtml > ${CMAKE_CURRENT_SOURCE_DIR}/doc/wsddn.8.html
        VERBATIM
    )
endif()

include(cmake/install.cmake)
