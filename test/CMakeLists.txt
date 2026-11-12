# SPDX-FileCopyrightText: Estonian Information System Authority
# SPDX-License-Identifier: LGPL-2.1-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)

# add_client_test(<name> SOURCES <files>... [CLIENT_SOURCES <files>...] [LIBRARIES <targets>...])
# Builds one Qt Test executable from the test sources and the listed client sources
# (relative to client/), so a test does not have to link the whole application.
function(add_client_test NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;CLIENT_SOURCES;LIBRARIES" ${ARGN})
    list(TRANSFORM ARG_CLIENT_SOURCES PREPEND ${PROJECT_SOURCE_DIR}/client/)
    add_executable(${NAME} ${ARG_SOURCES} ${ARG_CLIENT_SOURCES})
    target_include_directories(${NAME} PRIVATE ${PROJECT_SOURCE_DIR}/client)
    target_link_libraries(${NAME} PRIVATE Qt6::Test Qt6::Widgets ${ARG_LIBRARIES})
    add_test(NAME ${NAME} COMMAND ${NAME})
    # Widget tests run without a display (CI, SSH sessions)
    set_tests_properties(${NAME} PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endfunction()

add_client_test(InputValidatorTest
    SOURCES InputValidatorTest.cpp
    CLIENT_SOURCES IKValidator.cpp IKValidator.h
)

add_client_test(TLVTest
    SOURCES TLVTest.cpp
    CLIENT_SOURCES TLV.h
)
