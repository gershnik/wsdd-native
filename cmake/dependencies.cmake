# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(FetchContent)

if (DEFINED CACHE{libxml2_SOURCE_DIR} AND NOT DEFINED CACHE{WSDDN_DEPENDENCIES_VERSION})
    message(FATAL_ERROR 
    "Your existing CMake cache cannot be used due to incompatible changes."
    "Please delete ${CMAKE_BINARY_DIR}/CMakeCache.txt and rebuild. (sorry!)")
endif()
set(WSDDN_DEPENDENCIES_VERSION 1 CACHE INTERNAL "version of dependencies config")


if (NOT DEFINED WSDDN_PREFER_SYSTEM_LIBXML2)
    #By default prefer system libxml2 on Mac and source compiled on other platforms
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(WSDDN_PREFER_SYSTEM_LIBXML2 ON)
    else()
        set(WSDDN_PREFER_SYSTEM_LIBXML2 OFF)
    endif()
endif()

file(READ dependencies.json DEPENDECIES_JSON)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS dependencies.json)


set(DECLARED_DEPENDENCIES "")

function(fetch_dependency name #extras for FetchContent_Declare
)
    string(JSON version GET "${DEPENDECIES_JSON}" ${name} version)
    string(JSON url GET "${DEPENDECIES_JSON}" ${name} url)
    string(JSON md5 GET "${DEPENDECIES_JSON}" ${name} md5)
    string(REPLACE "\$\{version\}" ${version} url "${url}")
    string(TOUPPER ${name} uname)
    string(TOLOWER ${name} lname)
    
    set(extras "")
    foreach(i RANGE 1 ${ARGC})
        list(APPEND extras ${ARGV${i}})
    endforeach()

    if (CACHE{LAST_WSDDN_PREFER_SYSTEM_${uname}})
        set(old_prefer 1)
    else()
        set(old_prefer 0)
    endif()

    if (WSDDN_PREFER_SYSTEM_${uname})
        set(new_prefer 1)
    else()
        set(new_prefer 0)
    endif()
    
    if (NOT ${new_prefer} EQUAL ${old_prefer})
        unset(${lname}_POPULATED CACHE)
        unset(${lname}_SOURCE_DIR CACHE)
        unset(${lname}_BINARY_DIR CACHE)
        unset(${name}_FOUND CACHE)
    endif()
    set(LAST_WSDDN_PREFER_SYSTEM_${uname} ${WSDDN_PREFER_SYSTEM_${uname}} CACHE INTERNAL "")

    if (WSDDN_PREFER_SYSTEM_${uname})
        set(prefer_system FIND_PACKAGE_ARGS)
    else()
        set(prefer_system "")
    endif()
    FetchContent_Declare(${name}
        URL         ${url}
        URL_HASH    MD5=${md5}
        ${extras}
        ${prefer_system}
    )
    set(deplist ${DECLARED_DEPENDENCIES})
    list(APPEND deplist ${name})
    set(DECLARED_DEPENDENCIES ${deplist} PARENT_SCOPE)
endfunction()

#################################################

fetch_dependency(argum)
fetch_dependency(sys_string)
fetch_dependency(isptr)
fetch_dependency(ptl)
fetch_dependency(modern-uuid)


set(LIBXML2_WITH_ICONV OFF)
set(LIBXML2_WITH_LZMA OFF)
set(LIBXML2_WITH_HTML OFF)
set(LIBXML2_WITH_HTTP OFF)
set(LIBXML2_WITH_FTP OFF)
set(LIBXML2_WITH_TESTS OFF)
set(LIBXML2_WITH_ZLIB OFF)
set(LIBXML2_WITH_PYTHON OFF)
set(LIBXML2_WITH_LEGACY OFF)
set(LIBXML2_WITH_MODULES OFF)
set(LIBXML2_WITH_PROGRAMS OFF)

fetch_dependency(LibXml2)

set(FMT_INSTALL OFF)
fetch_dependency(fmt)

set(SPDLOG_NO_ATOMIC_LEVELS ON CACHE BOOL "prevent spdlog from using of std::atomic log levels (use only if your code never modifies log levels concurrently)")
set(SPDLOG_NO_TLS ON CACHE BOOL "prevent spdlog from using thread local storage")
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library instead of bundled")
fetch_dependency(spdlog)

fetch_dependency(tomlplusplus)
fetch_dependency(outcome
    SOURCE_SUBDIR   include #we don't really want to build it
)
fetch_dependency(asio)

#################################################

FetchContent_MakeAvailable(${DECLARED_DEPENDENCIES})

foreach(dep ${DECLARED_DEPENDENCIES})
    string(TOUPPER ${dep} udep)
    string(TOLOWER ${dep} ldep)
    if (DEFINED ${ldep}_SOURCE_DIR)
        message(STATUS "${dep} will be built from sources and statically linked")
    else()
        if (DEFINED ${ldep}_VERSION)
            message(STATUS "${dep} will be used from system (current version: ${${ldep}_VERSION})")
        else()
            if (DEFINED ${udep}_VERSION_STRING)
                message(STATUS "${dep} will be used from system (current version: ${${udep}_VERSION_STRING})")
            else()
                message(STATUS "${dep} will be used from system")
            endif()
        endif()
    endif()
endforeach()

get_directory_property(KNOWN_SUBDIRECTORIES SUBDIRECTORIES)
foreach(dir ${KNOWN_SUBDIRECTORIES})
    if (IS_DIRECTORY ${dir})
        foreach(dep ${DECLARED_DEPENDENCIES})
            if (DEFINED ${dep}_SOURCE_DIR)
                #check if the subdirectory is "under" the dependency source dir
                string(FIND ${dir} ${${dep}_SOURCE_DIR} match_pos)
                if (match_pos EQUAL 0)
                    #and, if so, exclude it from all to prevent installation
                    set_property(DIRECTORY ${dir} PROPERTY EXCLUDE_FROM_ALL YES)
                    break()
                endif()
            endif()
        endforeach()
    endif()
endforeach()
