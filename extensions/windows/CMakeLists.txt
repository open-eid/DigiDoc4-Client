add_library(EsteidShellExtensionV2 SHARED
    EsteidShellExtension.def
    EsteidShellExtensionV2.cpp
    EsteidShellExtension.rc
)
target_compile_definitions(EsteidShellExtensionV2 PRIVATE
    _UNICODE
    UNICODE
    _WINDLL
    WIN32_LEAN_AND_MEAN
    VERSION=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${BUILD_NUMBER}
    VERSION_STR="${VERSION}"
)
set_target_properties(EsteidShellExtensionV2 PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION YES
    COMPILE_OPTIONS "/guard:cf"
    LINK_OPTIONS "/guard:cf"
    LINK_LIBRARIES "uxtheme;RuntimeObject;Shlwapi"
    SKIP_AUTOMOC ON
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

add_custom_target(msixshellext DEPENDS EsteidShellExtensionV2
    COMMAND ${CMAKE_COMMAND} -E remove ${SHELLEXT_MSIX_FILE}
    COMMAND ${CMAKE_COMMAND} -E remove_directory appxshell
    COMMAND ${CMAKE_COMMAND} -E make_directory appxshell
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/AppxManifest-ShellExt.xml appxshell/AppxManifest.xml
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets appxshell/Assets
    COMMAND makeappx.exe pack /nv -d appxshell -p ${SHELLEXT_MSIX_FILE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(SIGNCERT)
    add_custom_command(TARGET EsteidShellExtensionV2 POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            /tr http://timestamp.digicert.com /td SHA256 $<TARGET_FILE:EsteidShellExtensionV2>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET msixshellext POST_BUILD
        COMMAND signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256 /du http://installer.id.ee
            /tr http://timestamp.digicert.com /td SHA256
            "${CMAKE_BINARY_DIR}/${SHELLEXT_MSIX_FILE}"
    )
endif()
