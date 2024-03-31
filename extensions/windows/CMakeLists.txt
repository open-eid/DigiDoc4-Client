project(EsteidShellExtension VERSION 3.13.9)
include(VersionInfo)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MIDL_TARGET "x64")
else()
    set(MIDL_TARGET "win32")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_i.h
           ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_i.c
    COMMAND Midl.Exe ${CMAKE_CURRENT_SOURCE_DIR}/EsteidShellExtension.idl
        /nologo /target NT100 /char signed /env ${MIDL_TARGET}
        /tlb EsteidShellExtension.tlb
        /h EsteidShellExtension_i.h
        /iid EsteidShellExtension_i.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

add_library(${PROJECT_NAME} SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/EsteidShellExtension_i.c
    dllmain.cpp
    EsteidShellExtension.def
    EsteidShlExt.cpp
    EsteidShellExtension.rc
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    COMPILE_DEFINITIONS "_UNICODE;UNICODE;_WINDLL"
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}
    COMPILE_OPTIONS "/guard:cf"
    LINK_OPTIONS "/guard:cf"
    LINK_LIBRARIES "uxtheme.lib"
    SKIP_AUTOMOC ON
)

if(SIGNCERT)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            /tr http://timestamp.digicert.com /td SHA256 $<TARGET_FILE:${PROJECT_NAME}>
    )
endif()
