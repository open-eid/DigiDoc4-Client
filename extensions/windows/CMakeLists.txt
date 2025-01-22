cmake_minimum_required(VERSION 3.16)
project(EsteidShellExtension VERSION 3.13.9)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM "x64")
else()
    set(PLATFORM "x86")
endif()

add_library(${PROJECT_NAME} SHARED
    dllmain.cpp
    EsteidShellExtension.def
    EsteidShlExt.cpp
    EsteidShellExtension.rc
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _UNICODE
    UNICODE
    _WINDLL
    WIN32_LEAN_AND_MEAN
    VERSION=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${BUILD_NUMBER}
    VERSION_STR="${VERSION}"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION YES
    COMPILE_OPTIONS "/guard:cf"
    LINK_OPTIONS "/guard:cf"
    LINK_LIBRARIES uxtheme.lib
    SKIP_AUTOMOC ON
)

add_custom_target(msishellext DEPENDS ${PROJECT_NAME}
    COMMAND wix.exe build -nologo
        -arch ${PLATFORM}
        -d ShellExt=$<TARGET_FILE:EsteidShellExtension>
        ${CMAKE_CURRENT_SOURCE_DIR}/EsteidShellExtension.wxs
        ${CMAKE_MODULE_PATH}/WelcomeDlg.wxs
        ${CMAKE_MODULE_PATH}/WixUI_Minimal.wxs
        -ext WixToolset.UI.wixext
        -bv WixUIDialogBmp=${CMAKE_MODULE_PATH}/dlgbmp.bmp
        -bv WixUIBannerBmp=${CMAKE_MODULE_PATH}/banner.bmp
        -o Digidoc_ShellExt-${VERSION}$ENV{VER_SUFFIX}.${PLATFORM}.msi
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(SIGNCERT)
    if(CROSSSIGNCERT)
        target_link_options(${PROJECT_NAME} PRIVATE "/INTEGRITYCHECK")
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            "$<$<BOOL:${CROSSSIGNCERT}>:/ph;/ac;${CROSSSIGNCERT}>"
            /tr http://timestamp.digicert.com /td SHA256 $<TARGET_FILE:EsteidShellExtension>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET msishellext POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            /tr http://timestamp.digicert.com /td SHA256
            "${CMAKE_BINARY_DIR}/Digidoc_ShellExt-${VERSION}$ENV{VER_SUFFIX}.${PLATFORM}.msi"
    )
endif()
