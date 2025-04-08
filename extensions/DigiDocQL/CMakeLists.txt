set(RESOURCES ../../client/mac/Resources/asic.icns)
add_executable(DigiDocQL MACOSX_BUNDLE
    ${RESOURCES}
    PreviewViewController.mm
)
set_target_properties(DigiDocQL PROPERTIES
    BUNDLE YES
    BUNDLE_EXTENSION appex
    RESOURCE ${RESOURCES}
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.cmake
    MACOSX_BUNDLE_BUNDLE_NAME DigiDocQL
    MACOSX_BUNDLE_GUI_IDENTIFIER "ee.ria.qdigidoc4.DigiDocQL"
    MACOSX_BUNDLE_COPYRIGHT "(C) 2010-2025 Estonian Information System Authority"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${BUILD_NUMBER}
    AUTOMOC OFF
    COMPILE_FLAGS "-Wno-unused-parameter -fobjc-arc"
    LINK_LIBRARIES "-framework QuickLookUI;-framework UniformTypeIdentifiers"
    LINK_FLAGS "-F/Library/Frameworks -fobjc-arc -e _NSExtensionMain -fapplication-extension"
    BUILD_WITH_INSTALL_RPATH YES
    INSTALL_RPATH "@loader_path/../../../../Frameworks"
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION appex
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "ee.ria.qdigidoc4.DigiDocQL"
)
target_link_libraries(DigiDocQL digidocpp::digidocpp)
