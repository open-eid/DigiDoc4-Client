cmake_minimum_required(VERSION 3.0)
if(POLICY CMP0043)
	cmake_policy(SET CMP0043 NEW)
endif()
if(POLICY CMP0063)
	cmake_policy(SET CMP0063 NEW)
endif()
project(qdigidoc4 VERSION 0.1.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

include( GNUInstallDirs )
include( VersionInfo )

find_package( PKCS11 )
find_package( LibDigiDocpp REQUIRED )
find_package( OpenSSL REQUIRED )
find_package( Qt5 COMPONENTS Core Widgets Network PrintSupport Svg LinguistTools REQUIRED )
include_directories( ${LIBDIGIDOCPP_INCLUDE_DIR} )

add_subdirectory( common )
add_subdirectory( client )
