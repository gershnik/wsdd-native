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

# install(CODE
#     "execute_process(COMMAND  gzip -f \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man8/wsddn.8\")"
# )

