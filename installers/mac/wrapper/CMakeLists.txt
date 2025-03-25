# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause


configure_file(Info.template.plist Info.plist @ONLY)

file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS Assets.xcassets/*)

set(GENERATED_ASSETS 
    ${CMAKE_CURRENT_BINARY_DIR}/assets/AppIcon.icns 
    ${CMAKE_CURRENT_BINARY_DIR}/assets/Assets.car 
)

add_custom_command(
    OUTPUT ${GENERATED_ASSETS}
    DEPENDS ${ASSET_FILES}
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/assets
    COMMAND actool --output-format human-readable-text --notices --warnings 
            --app-icon AppIcon --accent-color AccentColor 
            --output-partial-info-plist ${CMAKE_CURRENT_BINARY_DIR}/assets/PartialInfo.plist
            --enable-on-demand-resources NO --development-region en --target-device mac 
            --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET} --platform macosx 
            --compile ${CMAKE_CURRENT_BINARY_DIR}/assets 
            ${CMAKE_CURRENT_LIST_DIR}/Assets.xcassets 
    
)

add_executable(wrapper MACOSX_BUNDLE)

set_target_properties(wrapper PROPERTIES
    OUTPUT_NAME wsdd-native
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    C_STANDARD 11
    CXX_STANDARD_REQUIRED True
    C_STANDARD_REQUIRED True
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
)

target_link_libraries(wrapper
PRIVATE
    "-framework Foundation"
    "-framework CoreServices"
)

target_sources(wrapper
PRIVATE
    main.mm
    ${GENERATED_ASSETS}
)
set_source_files_properties(${GENERATED_ASSETS} PROPERTIES 
    MACOSX_PACKAGE_LOCATION Resources
)

target_link_options(wrapper 
PRIVATE
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-object_path_lto,${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lto.o>"
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/LTOCache>"
    "$<$<CXX_COMPILER_ID:AppleClang>:-Wl,-no_adhoc_codesign>"
    "$<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,13.5>>:-Wl,-reproducible>"
    "$<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15.0>>:LINKER:-no_warn_duplicate_libraries>" #see https://gitlab.kitware.com/cmake/cmake/-/issues/25297
)

if (CMAKE_GENERATOR MATCHES "Xcode")
    set_target_properties(wrapper PROPERTIES
        XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS YES
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${WSDDN_BUNDLE_IDENTIFIER}.wrapper
    )
else()
    add_custom_command(TARGET wrapper POST_BUILD
        COMMAND dsymutil "$<TARGET_FILE:wrapper>" -o "$<TARGET_BUNDLE_DIR:wrapper>.dSYM"
        COMMAND codesign --force --sign - --timestamp=none 
                     --options runtime "$<TARGET_BUNDLE_DIR:wrapper>"
    )
endif()