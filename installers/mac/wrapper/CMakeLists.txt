# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause


configure_file(Info.template.plist Info.plist @ONLY)

add_custom_target(populate_wrapper
    COMMENT "Populating wrapper bundle"
    DEPENDS wsddn manpage
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/populate.py "$<TARGET_BUNDLE_DIR:wrapper>" 
        --identifier ${WSDDN_BUNDLE_IDENTIFIER}
        --wsddn-dir "$<TARGET_FILE_DIR:wsddn>"
        --doc-dir "${CMAKE_BINARY_DIR}/doc"
)

add_executable(wrapper MACOSX_BUNDLE)

add_dependencies(wrapper populate_wrapper)

set_target_properties(wrapper PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    C_STANDARD 11
    CXX_STANDARD_REQUIRED True
    C_STANDARD_REQUIRED True
    OUTPUT_NAME "wsddn"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
)

target_link_libraries(wrapper
PRIVATE
    argum
    sys_string
    "-framework Foundation"
    "-framework CoreServices"
)

target_compile_definitions(wrapper
PRIVATE
    ARGUM_USE_EXPECTED=1
)

target_sources(wrapper
PRIVATE
    main.mm
)

target_link_options(wrapper 
PRIVATE
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-object_path_lto,${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lto.o>"
)

if (CMAKE_GENERATOR MATCHES "Xcode")
    set_target_properties(wrapper PROPERTIES
        XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS YES
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym
    )
else()
    add_custom_command(TARGET wrapper POST_BUILD
        COMMAND dsymutil "$<TARGET_FILE:wrapper>" -o "$<TARGET_BUNDLE_DIR:wrapper>.dSYM"
        COMMAND codesign --force --sign - --timestamp=none 
                     --options runtime "$<TARGET_BUNDLE_DIR:wrapper>" 
                     --entitlements ${CMAKE_CURRENT_LIST_DIR}/wrapper.entitlements
    )
endif()