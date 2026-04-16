cmake_minimum_required(VERSION 3.16)
project(EsteidShellExtension VERSION 3.13.9)
add_library(${PROJECT_NAME} SHARED
    EsteidShellExtension.def
    EsteidShellExtension.cpp
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
    LINK_LIBRARIES "uxtheme;RuntimeObject;Shlwapi"
    SKIP_AUTOMOC ON
)

add_library(${PROJECT_NAME}V2 SHARED
    EsteidShellExtension.def
    EsteidShellExtensionV2.cpp
    EsteidShellExtension.rc
)
target_compile_definitions(${PROJECT_NAME}V2 PRIVATE
    _UNICODE
    UNICODE
    _WINDLL
    WIN32_LEAN_AND_MEAN
    NO_ICON
    VERSION=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${BUILD_NUMBER}
    VERSION_STR="${VERSION}"
)
set_target_properties(${PROJECT_NAME}V2 PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION YES
    COMPILE_OPTIONS "/guard:cf"
    LINK_OPTIONS "/guard:cf"
    LINK_LIBRARIES "uxtheme;RuntimeObject;Shlwapi"
    SKIP_AUTOMOC ON
)

add_custom_target(msishellext DEPENDS ${PROJECT_NAME}
    COMMAND wix.exe build -nologo
        -arch $ENV{PLATFORM}
        -d ShellExt=$<TARGET_FILE:EsteidShellExtension>
        ${CMAKE_CURRENT_SOURCE_DIR}/EsteidShellExtension.wxs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/WelcomeDlg.wxs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/WixUI_Minimal.wxs
        -ext WixToolset.UI.wixext
        -bv WixUIDialogBmp=${CMAKE_CURRENT_SOURCE_DIR}/../../common/dlgbmp.bmp
        -bv WixUIBannerBmp=${CMAKE_CURRENT_SOURCE_DIR}/../../common/banner.bmp
        -o Digidoc_ShellExt-${VERSION}$ENV{VER_SUFFIX}.$ENV{PLATFORM}.msi
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set(PLATFORM $ENV{PLATFORM})
if(SIGNCERT)
    execute_process(
        COMMAND powershell -NoProfile -NonInteractive -Command
            "(Get-ChildItem Cert:\\CurrentUser\\My | Where-Object { $_.Subject -like '*${SIGNCERT}*' } | Select-Object -First 1).Subject"
        OUTPUT_VARIABLE SIGNCERT_SUBJECT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
configure_file(${CMAKE_SOURCE_DIR}/AppxManifest-ShellExt.xml.cmake ${CMAKE_BINARY_DIR}/AppxManifest-ShellExt.xml.in)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/AppxManifest-ShellExt.xml INPUT ${CMAKE_BINARY_DIR}/AppxManifest-ShellExt.xml.in)
set(SHELLEXT_MSIX_FILE "DigiDoc4Shell.msix")
set(SHELLEXT_MSIX_FILE ${SHELLEXT_MSIX_FILE} PARENT_SCOPE)

add_custom_target(msixshellext DEPENDS ${PROJECT_NAME}V2
    COMMAND ${CMAKE_COMMAND} -E remove ${SHELLEXT_MSIX_FILE}
    COMMAND ${CMAKE_COMMAND} -E remove_directory appxshell
    COMMAND ${CMAKE_COMMAND} -E make_directory appxshell
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/AppxManifest-ShellExt.xml appxshell/AppxManifest.xml
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets appxshell/Assets
    COMMAND makeappx.exe pack /nv -d appxshell -p ${SHELLEXT_MSIX_FILE}
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
            "${CMAKE_BINARY_DIR}/Digidoc_ShellExt-${VERSION}$ENV{VER_SUFFIX}.$ENV{PLATFORM}.msi"
    )
    add_custom_command(TARGET ${PROJECT_NAME}V2 POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            "$<$<BOOL:${CROSSSIGNCERT}>:/ph;/ac;${CROSSSIGNCERT}>"
            /tr http://timestamp.digicert.com /td SHA256 $<TARGET_FILE:EsteidShellExtensionV2>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET msixshellext POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            /tr http://timestamp.digicert.com /td SHA256
            "${CMAKE_BINARY_DIR}/${SHELLEXT_MSIX_FILE}"
    )
endif()
