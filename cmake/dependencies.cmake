# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(FetchContent)
include(ExternalProject)

set(DECLARED_DEPENDENCIES "")

FetchContent_Declare(argum
    GIT_REPOSITORY  https://github.com/gershnik/argum.git
    GIT_TAG         v2.2
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
list(APPEND DECLARED_DEPENDENCIES argum)

FetchContent_Declare(sys_string
    GIT_REPOSITORY  https://github.com/gershnik/sys_string.git
    GIT_TAG         v2.4
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
    SOURCE_SUBDIR   lib
)
list(APPEND DECLARED_DEPENDENCIES sys_string)

FetchContent_Declare(isptr
    GIT_REPOSITORY  https://github.com/gershnik/intrusive_shared_ptr.git
    GIT_TAG         v1.1
    GIT_PROGRESS    TRUE
    GIT_SHALLOW     TRUE
)
list(APPEND DECLARED_DEPENDENCIES isptr)

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

    FetchContent_Declare(libxml2
        GIT_REPOSITORY  https://gitlab.gnome.org/GNOME/libxml2.git
        GIT_TAG         v2.9.14
        GIT_SHALLOW     TRUE
        GIT_PROGRESS    TRUE
    )
    list(APPEND DECLARED_DEPENDENCIES libxml2)

endif()

FetchContent_Declare(libuuid
    GIT_REPOSITORY  https://github.com/gershnik/libuuid-cmake.git
    GIT_TAG         v2.38
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
list(APPEND DECLARED_DEPENDENCIES libuuid)

set(SPDLOG_NO_ATOMIC_LEVELS ON)
set(SPDLOG_NO_TLS ON)

FetchContent_Declare(spdlog
    GIT_REPOSITORY  https://github.com/gabime/spdlog
    GIT_TAG         v1.10.0
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
list(APPEND DECLARED_DEPENDENCIES spdlog)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY  https://github.com/marzer/tomlplusplus.git
    GIT_TAG         v3.1.0
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
    GIT_SUBMODULES_RECURSE FALSE
)
list(APPEND DECLARED_DEPENDENCIES tomlplusplus)

# FetchContent_Declare(outcome
#     GIT_REPOSITORY  https://github.com/ned14/outcome
#     GIT_TAG         v2.2.3
#     GIT_SHALLOW     TRUE
#     SOURCE_SUBDIR   include #we don't really want to build it
# )
FetchContent_Declare(outcome
    URL             https://github.com/ned14/outcome/tarball/v2.2.3
    SOURCE_SUBDIR   include #we don't really want to build it
)
list(APPEND DECLARED_DEPENDENCIES outcome)

FetchContent_Declare(asio
    URL             https://sourceforge.net/projects/asio/files/asio/1.22.1%20%28Stable%29/asio-1.22.1.tar.gz/download
    URL_HASH        MD5=dd2befc3950bea6e7e0d2898f88e46af
)
list(APPEND DECLARED_DEPENDENCIES asio)

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
