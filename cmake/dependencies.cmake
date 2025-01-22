# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(FetchContent)
include(ExternalProject)

set(DECLARED_DEPENDENCIES "")
set(DEPENDECIES_JSON "")

#################################################

set(ARGUM_REPO gershnik/argum)
set(ARGUM_VER v2.5)
FetchContent_Declare(argum
    GIT_REPOSITORY  https://github.com/${ARGUM_REPO}.git
    GIT_TAG         ${ARGUM_VER}
    GIT_SHALLOW     TRUE
)
list(APPEND DECLARED_DEPENDENCIES argum)
list(APPEND DEPENDECIES_JSON "\"argum\": \"pkg:github/${ARGUM_REPO}@${ARGUM_VER}\"")

#################################################

set(SYS_STRING_REPO gershnik/sys_string)
set(SYS_STRING_VER v2.16)
FetchContent_Declare(sys_string
    GIT_REPOSITORY  https://github.com/${SYS_STRING_REPO}.git
    GIT_TAG         ${SYS_STRING_VER}
    GIT_SHALLOW     TRUE
)
list(APPEND DECLARED_DEPENDENCIES sys_string)
list(APPEND DEPENDECIES_JSON "\"sys_string\": \"pkg:github/${SYS_STRING_REPO}@${SYS_STRING_VER}\"")

#################################################

set(ISPTR_REPO gershnik/intrusive_shared_ptr)
set(ISPTR_VER v1.5)
FetchContent_Declare(isptr
    GIT_REPOSITORY  https://github.com/${ISPTR_REPO}.git
    GIT_TAG         ${ISPTR_VER}
    GIT_SHALLOW     TRUE
)
list(APPEND DECLARED_DEPENDENCIES isptr)
list(APPEND DEPENDECIES_JSON "\"isptr\": \"pkg:github/${ISPTR_REPO}@${ISPTR_VER}\"")

#################################################

set(PTL_REPO gershnik/ptl)
set(PTL_VER v1.3)
FetchContent_Declare(ptl
    GIT_REPOSITORY  https://github.com/${PTL_REPO}.git
    GIT_TAG         ${PTL_VER}
    GIT_SHALLOW     TRUE
)
list(APPEND DECLARED_DEPENDENCIES ptl)
list(APPEND DEPENDECIES_JSON "\"ptl\": \"pkg:github/${PTL_REPO}@${PTL_VER}\"")

#################################################

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

    set(LIBXML_VER v2.13.5)
    FetchContent_Declare(libxml2
        GIT_REPOSITORY  https://gitlab.gnome.org/GNOME/libxml2.git
        GIT_TAG         ${LIBXML_VER}
        GIT_SHALLOW     TRUE
    )
    list(APPEND DECLARED_DEPENDENCIES libxml2)
    list(APPEND DEPENDECIES_JSON "\"libxml2\": \"pkg:generic/libxml2@${LIBXML_VER}\"")

endif()

#################################################

set(MUUID_REPO gershnik/modern-uuid)
set(MUUID_VER v1.0)
FetchContent_Declare(modern-uuid
    GIT_REPOSITORY  https://github.com/${MUUID_REPO}.git
    GIT_TAG         ${MUUID_VER}
    GIT_SHALLOW     TRUE
)
list(APPEND DECLARED_DEPENDENCIES modern-uuid)
list(APPEND DEPENDECIES_JSON "\"modern-uuid\": \"pkg:github/${MUUID_REPO}@${MUUID_VER}\"")

#################################################

set(FMT_INSTALL OFF)

set(FMT_REPO fmtlib/fmt)
set(FMT_VER 11.1.1)
FetchContent_Declare(fmt
    GIT_REPOSITORY  https://github.com/${FMT_REPO}
    GIT_TAG         ${FMT_VER}
    GIT_SHALLOW     TRUE
    GIT_SUBMODULES_RECURSE FALSE
)
list(APPEND DECLARED_DEPENDENCIES fmt)
list(APPEND DEPENDECIES_JSON "\"fmt\": \"pkg:github/${FMT_REPO}@${FMT_VER}\"")

#################################################

set(SPDLOG_NO_ATOMIC_LEVELS ON CACHE BOOL "prevent spdlog from using of std::atomic log levels (use only if your code never modifies log levels concurrently)")
set(SPDLOG_NO_TLS ON CACHE BOOL "prevent spdlog from using thread local storage")
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library instead of bundled")

set(SPDLOG_REPO gabime/spdlog)
set(SPDLOG_VER v1.15.0)
FetchContent_Declare(spdlog
    # GIT_REPOSITORY  https://github.com/${SPDLOG_REPO}
    # GIT_TAG         ${SPDLOG_VER}
    # GIT_SHALLOW     TRUE

    URL              https://github.com/${SPDLOG_REPO}/tarball/${SPDLOG_VER}

    PATCH_COMMAND    patch -p0 -s -f -i ${CMAKE_CURRENT_LIST_DIR}/patches/spdlog.diff 
    LOG_PATCH        ON
)
list(APPEND DECLARED_DEPENDENCIES spdlog)
list(APPEND DEPENDECIES_JSON "\"spdlog\": \"pkg:github/${SPDLOG_REPO}@${SPDLOG_VER}\"")

#################################################

set(TOMPLUSPLUS_REPO marzer/tomlplusplus)
set(TOMPLUSPLUS_VER v3.4.0)
# FetchContent_Declare(tomlplusplus
#     GIT_REPOSITORY  https://github.com/${TOMPLUSPLUS_REPO}.git
#     GIT_TAG         ${TOMPLUSPLUS_VER}
#     GIT_SHALLOW     TRUE
#     GIT_SUBMODULES_RECURSE FALSE
# )
FetchContent_Declare(tomlplusplus
    URL             https://github.com/${TOMPLUSPLUS_REPO}/tarball/${TOMPLUSPLUS_VER}
)
list(APPEND DECLARED_DEPENDENCIES tomlplusplus)
list(APPEND DEPENDECIES_JSON "\"tomlplusplus\": \"pkg:github/${TOMPLUSPLUS_REPO}@${TOMPLUSPLUS_VER}\"")

#################################################

set(OUTCOME_REPO ned14/outcome)
set(OUTCOME_VER v2.2.11)
# FetchContent_Declare(outcome
#     GIT_REPOSITORY  https://github.com/${OUTCOME_REPO}
#     GIT_TAG         ${OUTCOME_VER}
#     GIT_SHALLOW     TRUE
#     SOURCE_SUBDIR   include #we don't really want to build it
# )
FetchContent_Declare(outcome
    URL             https://github.com/${OUTCOME_REPO}/tarball/${OUTCOME_VER}
    SOURCE_SUBDIR   include #we don't really want to build it
)
list(APPEND DECLARED_DEPENDENCIES outcome)
list(APPEND DEPENDECIES_JSON "\"outcome\": \"pkg:github/${OUTCOME_REPO}@${OUTCOME_VER}\"")

#################################################

set(ASIO_VER 1.30.2)
set(ASIO_URL https://sourceforge.net/projects/asio/files/asio/${ASIO_VER}%20%28Stable%29/asio-${ASIO_VER}.tar.gz/download)
set(ASIO_CHECKSUM c1643d3eddd45b210b760acc7ec25d59)
FetchContent_Declare(asio
    URL             ${ASIO_URL}
    URL_HASH        MD5=${ASIO_CHECKSUM}
)
list(APPEND DECLARED_DEPENDENCIES asio)
list(APPEND DEPENDECIES_JSON "\"asio\": \"pkg:generic/asio@${ASIO_VER}?download_url=${ASIO_URL}&checksum=md5:${ASIO_CHECKSUM}\"")

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

list(JOIN DEPENDECIES_JSON ",\n  " DEPENDECIES_JSON)
cmake_path(RELATIVE_PATH CMAKE_CURRENT_LIST_FILE OUTPUT_VARIABLE JSON_SRC_PATH)
set(DEPENDECIES_JSON "{
\"version\": \"1.0\",
\"src\": \"${JSON_SRC_PATH}\",
\"dependencies\": {
  ${DEPENDECIES_JSON}
}}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dependencies.json ${DEPENDECIES_JSON})

