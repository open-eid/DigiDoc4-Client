cmake_minimum_required(VERSION 3.10)
if(POLICY CMP0074)
	cmake_policy(SET CMP0074 NEW)
endif()
project(qdigidoc4 VERSION 4.2.9)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

include( GNUInstallDirs )
include( VersionInfo )

find_package( PKCS11 )
find_package( LibDigiDocpp 3.14.5 REQUIRED )
find_package( LDAP REQUIRED )
find_package( Qt5 5.9.0 REQUIRED COMPONENTS Core Widgets Network PrintSupport Svg LinguistTools )

set_env( TSL_URL "https://ec.europa.eu/tools/lotl/eu-lotl.xml" CACHE STRING "TSL trust list primary URL" )
set_env( TSL_INCLUDE "EE" CACHE STRING "TSL list include in binary" )
set_env( MOBILEID_URL "https://dd-mid.ria.ee/mid-api" CACHE STRING "URL for Mobile-ID" )
set_env( SMARTID_URL "https://dd-sid.ria.ee/v1" CACHE STRING "URL for Smart-ID" )
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION YES)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG NO)
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "qdigidoc4")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "Andrus Randveer")
set(CPACK_PACKAGE_VENDOR "RIA")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        "/usr/local" "/usr/local/etc" "/usr/share/man"
        "/usr/share/man/man1" "/usr/lib64/pkgconfig" "/usr/share/icons/hicolor"
        "/usr/share/applications" "/usr/share/icons" "/usr/share/mime" "/usr/share/mime/packages"
        "/usr/share/icons/hicolor/16x16" "/usr/share/icons/hicolor/16x16/apps" "/usr/share/icons/hicolor/16x16/mimetypes"
        "/usr/share/icons/hicolor/22x22" "/usr/share/icons/hicolor/22x22/apps" "/usr/share/icons/hicolor/22x22/mimetypes"
        "/usr/share/icons/hicolor/32x32" "/usr/share/icons/hicolor/32x32/apps" "/usr/share/icons/hicolor/32x32/mimetypes"
        "/usr/share/icons/hicolor/48x48" "/usr/share/icons/hicolor/48x48/apps" "/usr/share/icons/hicolor/48x48/mimetypes"
        "/usr/share/icons/hicolor/128x128" "/usr/share/icons/hicolor/128x128/apps" "/usr/share/icons/hicolor/128x128/mimetypes"
        "/usr/share/icons/hicolor/256x256" "/usr/share/icons/hicolor/256x256/apps" "/usr/share/icons/hicolor/256x256/mimetypes"
        "/usr/share/icons/hicolor/512x512" "/usr/share/icons/hicolor/512x512/apps" "/usr/share/icons/hicolor/512x512/mimetypes"
        "/usr/share/locale/et" "/usr/share/locale/et/LC_MESSAGES"
        "/usr/share/locale/ru" "/usr/share/locale/ru/LC_MESSAGES"
        "/usr/local/share" "/usr/share/locale"
    )
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)
if(APPLE)
    add_subdirectory(extensions/DigiDocQL)
elseif(WIN32)
    add_subdirectory(extensions/windows EXCLUDE_FROM_ALL)
elseif(UNIX)
    option(ENABLE_KDE "Install KDE service menu (default: TRUE)" TRUE)
    option(ENABLE_NAUTILUS_EXTENSION "Build Nautilus extension (default: TRUE)" TRUE)
    if (ENABLE_KDE)
        add_subdirectory(extensions/kde)
    endif()
    if (ENABLE_NAUTILUS_EXTENSION)
        add_subdirectory(extensions/nautilus)
    endif()
endif()

add_subdirectory( common )
add_subdirectory( client )
