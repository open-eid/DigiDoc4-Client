cmake_minimum_required(VERSION 3.16)
project(digidocshellextension VERSION 3.13.9)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include( VersionInfo )

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MIDL_TARGET "x64")
    set(PLATFORM "x64")
else()
    set(MIDL_TARGET "win32")
    set(PLATFORM "x86")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_i.h
           ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_i.c
           ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_p.c
    COMMAND Midl.Exe ${CMAKE_CURRENT_SOURCE_DIR}/EsteidShellExtension.idl
        /nologo /no_robust /char signed /Oicf /env ${MIDL_TARGET}
        /I ${CMAKE_CURRENT_SOURCE_DIR}
        /tlb EsteidShellExtension.tlb
        /h EsteidShellExtension_i.h
        /iid EsteidShellExtension_i.c
        /proxy EsteidShellExtension_p.c 2> nul
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

add_library(EsteidShellExtension SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_i.c
    dllmain.cpp
    EsteidShellExtension.cpp
    EsteidShellExtension.def
    EsteidShlExt.cpp
    stdafx.cpp
    EsteidShellExtension.rc
    EsteidShellExtension.rgs
    EsteidShlExt_x86.rgs
    EsteidShlExt_x64.rgs
)
set_target_properties(EsteidShellExtension PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    COMPILE_DEFINITIONS "_UNICODE;UNICODE;_MERGE_PROXYSTUB;_WINDLL"
    COMPILE_OPTIONS "/guard:cf"
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}
    INTERPROCEDURAL_OPTIMIZATION YES
    LINK_OPTIONS "/guard:cf"
    LINK_LIBRARIES "uxtheme.lib"
    SKIP_AUTOMOC ON
)

add_custom_target(msishellext DEPENDS EsteidShellExtension
    COMMAND wix.exe build -nologo
        -arch ${PLATFORM}
        -d MSI_VERSION=${VERSION}
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
        target_link_options(EsteidShellExtension PRIVATE "/INTEGRITYCHECK")
    endif()
    add_custom_command(TARGET EsteidShellExtension POST_BUILD
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
