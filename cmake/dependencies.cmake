# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(FetchContent)

file(READ dependencies.json DEPENDECIES_JSON)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS dependencies.json)


set(DECLARED_DEPENDENCIES "")

function(fetch_dependency name #extras for FetchContent_Declare
)
    string(JSON version GET "${DEPENDECIES_JSON}" ${name} version)
    string(JSON url GET "${DEPENDECIES_JSON}" ${name} url)
    string(JSON md5 GET "${DEPENDECIES_JSON}" ${name} md5)
    string(REPLACE "\$\{version\}" ${version} url "${url}")
    set(extras "")
    foreach(i RANGE 1 ${ARGC})
        list(APPEND extras ${ARGV${i}})
    endforeach()
    FetchContent_Declare(${name}
        URL         ${url}
        URL_HASH    MD5=${md5}
        ${extras}
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

if (WSDDN_PREFER_SYSTEM)
    find_package(LibXml2)
endif()

if (NOT LibXml2_FOUND)

    message(STATUS "libxm2 will be built from sources and statically linked")

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

    fetch_dependency(libxml2)
    
endif()


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

get_directory_property(KNOWN_SUBDIRECTORIES SUBDIRECTORIES)
foreach(dir ${KNOWN_SUBDIRECTORIES})
    if (IS_DIRECTORY ${dir})
        foreach(dep ${DECLARED_DEPENDENCIES})
            #check if the subdirectory is "under" the dependency source dir
            string(FIND ${dir} ${${dep}_SOURCE_DIR} match_pos)
            if (match_pos EQUAL 0)
                #and, if so, exclude it from all to prevent installation
                set_property(DIRECTORY ${dir} PROPERTY EXCLUDE_FROM_ALL YES)
                break()
            endif()
        endforeach()
    endif()
endforeach()


