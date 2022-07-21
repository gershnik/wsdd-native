# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(GNUInstallDirs)

install(CODE "message(\"Prefix: '\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}'\")")

install(TARGETS 
    wsddn RUNTIME 
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/doc/wsddn.8.gz
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    install(CODE "
        if(CMAKE_INSTALL_DO_STRIP)
            execute_process(COMMAND codesign --force --sign - --timestamp=none \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/wsddn\")
        endif()
    ")
endif()

