# SPDX-FileCopyrightText: Estonian Information System Authority
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# SBOM generation using DEMCON/cmake-sbom (SPDX 2.3, install-time)
# Run: cmake --install <build-dir>/sbom

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/cmake-sbom/cmake")
include(sbom)

sbom_generate(
    OUTPUT "${CMAKE_BINARY_DIR}/qdigidoc4-${PROJECT_VERSION}.spdx"
    LICENSE "LGPL-2.1-or-later"
    SUPPLIER "Estonian Information System Authority"
    SUPPLIER_URL https://www.ria.ee
    DOWNLOAD_URL https://github.com/open-eid/DigiDoc4-Client
    VERSION "${PROJECT_VERSION}"
)

set(_sbom_reset "${CMAKE_BINARY_DIR}/sbom/sbom-reset.cmake")
file(WRITE "${_sbom_reset}"
    "file(WRITE \"${CMAKE_BINARY_DIR}/sbom/sbom.spdx.in\" \"\")\n"
    "file(READ \"${CMAKE_BINARY_DIR}/SPDXRef-DOCUMENT.spdx.in\" _doc)\n"
    "file(APPEND \"${CMAKE_BINARY_DIR}/sbom/sbom.spdx.in\" \"\${_doc}\")\n"
    "set(SBOM_VERIFICATION_CODES \"\")\n"
)
file(APPEND "${CMAKE_BINARY_DIR}/sbom/CMakeLists.txt"
    "install(SCRIPT \"${_sbom_reset}\")\n"
)

set(_app_spdxid "SPDXRef-Package-${PROJECT_NAME} DEPENDS_ON @SBOM_LAST_SPDXID@")

if(APPLE)
    find_program(OPENSC_TOOL NAMES opensc-tool HINTS /Library/OpenSC/bin)
    if(OPENSC_TOOL)
        execute_process(
            COMMAND "${OPENSC_TOOL}" --version
            OUTPUT_VARIABLE OPENSC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        string(REGEX REPLACE "^OpenSC-([0-9.]+).*" "\\1" OPENSC_VERSION "${OPENSC_VERSION}")
    endif()
    if(OPENSC_VERSION)
        sbom_add(PACKAGE OpenSC
            VERSION "${OPENSC_VERSION}"
            SUPPLIER "Organization: OpenSC Project"
            DOWNLOAD_LOCATION https://github.com/OpenSC/OpenSC
            LICENSE "LGPL-2.1-or-later"
            EXTREF "cpe:2.3:a:opensc-project:opensc:${OPENSC_VERSION}:*:*:*:*:*:*:*"
            RELATIONSHIP "${_app_spdxid}"
        )
    endif()
    sbom_add(PACKAGE DigiDocQL
        VERSION "${PROJECT_VERSION}"
        SUPPLIER "Organization: Estonian Information System Authority"
        DOWNLOAD_LOCATION https://github.com/open-eid/DigiDoc4-Client
        LICENSE "LGPL-2.1-or-later"
        RELATIONSHIP "@SBOM_LAST_SPDXID@ VARIANT_OF SPDXRef-Package-${PROJECT_NAME}"
    )
    set(_app_spdxid "${_app_spdxid}\nRelationship: ${SBOM_LAST_SPDXID} DEPENDS_ON @SBOM_LAST_SPDXID@")
elseif(WIN32)
    sbom_add(PACKAGE EsteidShellExtension
        VERSION "3.13.9"
        SUPPLIER "Organization: Estonian Information System Authority"
        DOWNLOAD_LOCATION https://github.com/open-eid/DigiDoc4-Client
        LICENSE "LGPL-2.1-or-later"
        RELATIONSHIP "@SBOM_LAST_SPDXID@ VARIANT_OF SPDXRef-Package-${PROJECT_NAME}"
    )
    set(_app_spdxid "${_app_spdxid}\nRelationship: ${SBOM_LAST_SPDXID} DEPENDS_ON @SBOM_LAST_SPDXID@")
    find_program(WIX_EXECUTABLE NAMES wix)
    if(WIX_EXECUTABLE)
        execute_process(
            COMMAND "${WIX_EXECUTABLE}" --version
            OUTPUT_VARIABLE WIX_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        string(REGEX REPLACE "\\+.*$" "" WIX_VERSION "${WIX_VERSION}")
    endif()
    if(WIX_VERSION)
        sbom_add(PACKAGE WiX
            VERSION "${WIX_VERSION}"
            SUPPLIER "Organization: WiX Toolset Contributors"
            DOWNLOAD_LOCATION https://wixtoolset.org
            LICENSE "MS-RL"
            EXTREF "cpe:2.3:a:wixtoolset:wix_toolset:${WIX_VERSION}:*:*:*:*:*:*:*"
            RELATIONSHIP "@SBOM_LAST_SPDXID@ BUILD_TOOL_OF SPDXRef-Package-${PROJECT_NAME}"
        )
    endif()
    find_package(LibXml2 QUIET)
    if(LibXml2_FOUND)
        sbom_add(PACKAGE libxml2
            VERSION "${LIBXML2_VERSION_STRING}"
            SUPPLIER "Organization: GNOME Project"
            DOWNLOAD_LOCATION https://gitlab.gnome.org/GNOME/libxml2
            LICENSE "MIT"
            EXTREF "cpe:2.3:a:xmlsoft:libxml2:${LIBXML2_VERSION_STRING}:*:*:*:*:*:*:*"
            RELATIONSHIP "${_app_spdxid}"
        )
    endif()
    find_package(LibXmlSec QUIET)
    if(LibXmlSec_FOUND)
        sbom_add(PACKAGE xmlsec1
            VERSION "${LibXmlSec_VERSION}"
            SUPPLIER "Organization: Aleksey Sanin"
            DOWNLOAD_LOCATION https://www.aleksey.com/xmlsec/
            LICENSE "MIT"
            EXTREF "cpe:2.3:a:xmlsec_project:xmlsec:${LibXmlSec_VERSION}:*:*:*:*:*:*:*"
            RELATIONSHIP "${_app_spdxid}"
        )
    endif()
elseif(UNIX)
    if(ENABLE_NAUTILUS_EXTENSION)
        sbom_add(PACKAGE nautilus-qdigidoc
            VERSION "${PROJECT_VERSION}"
            SUPPLIER "Person: Erkko Kebbinau"
            DOWNLOAD_LOCATION https://github.com/open-eid/DigiDoc4-Client
            LICENSE "LGPL-2.1-or-later"
            RELATIONSHIP "@SBOM_LAST_SPDXID@ VARIANT_OF SPDXRef-Package-${PROJECT_NAME}"
        )
        set(_app_spdxid "${_app_spdxid}\nRelationship: ${SBOM_LAST_SPDXID} DEPENDS_ON @SBOM_LAST_SPDXID@")
    endif()
    if(ENABLE_KDE)
        sbom_add(PACKAGE qdigidoc-signer-kde
            VERSION "${PROJECT_VERSION}"
            SUPPLIER "Organization: Estonian Information System Authority"
            DOWNLOAD_LOCATION https://github.com/open-eid/DigiDoc4-Client
            LICENSE "LGPL-2.1-or-later"
            RELATIONSHIP "@SBOM_LAST_SPDXID@ VARIANT_OF SPDXRef-Package-${PROJECT_NAME}"
        )
        set(_app_spdxid "${_app_spdxid}\nRelationship: ${SBOM_LAST_SPDXID} DEPENDS_ON @SBOM_LAST_SPDXID@")
    endif()
    find_package(Gettext QUIET)
    if(GETTEXT_FOUND)
        sbom_add(PACKAGE Gettext
            VERSION "${GETTEXT_VERSION_STRING}"
            SUPPLIER "Organization: Free Software Foundation"
            DOWNLOAD_LOCATION https://www.gnu.org/software/gettext/
            LICENSE "GPL-3.0-or-later"
            EXTREF "cpe:2.3:a:gnu:gettext:${GETTEXT_VERSION_STRING}:*:*:*:*:*:*:*"
            RELATIONSHIP "@SBOM_LAST_SPDXID@ BUILD_TOOL_OF SPDXRef-Package-${PROJECT_NAME}"
        )
    endif()
endif()

sbom_add(PACKAGE libdigidocpp
    VERSION "${libdigidocpp_VERSION}"
    SUPPLIER "Organization: Estonian Information System Authority"
    DOWNLOAD_LOCATION https://github.com/open-eid/libdigidocpp
    LICENSE "LGPL-2.1-or-later"
    EXTREF "cpe:2.3:a:ria:libdigidocpp:${libdigidocpp_VERSION}:*:*:*:*:*:*:*"
    RELATIONSHIP "${_app_spdxid}"
)

sbom_add(PACKAGE Qt6
    VERSION "${Qt6_VERSION}"
    SUPPLIER "Organization: The Qt Company"
    DOWNLOAD_LOCATION https://download.qt.io/
    LICENSE "LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only OR LicenseRef-Qt-commercial"
    EXTREF "cpe:2.3:a:qt:qt:${Qt6_VERSION}:*:*:*:*:*:*:*"
    RELATIONSHIP "${_app_spdxid}"
)

find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    sbom_add(PACKAGE OpenSSL
        VERSION "${OPENSSL_VERSION}"
        SUPPLIER "Organization: OpenSSL Software Foundation"
        DOWNLOAD_LOCATION https://openssl.org
        LICENSE "Apache-2.0"
        EXTREF "cpe:2.3:a:openssl:openssl:${OPENSSL_VERSION}:*:*:*:*:*:*:*"
        RELATIONSHIP "${_app_spdxid}"
    )
endif()

if(FlatBuffers_VERSION)
    sbom_add(PACKAGE FlatBuffers
        VERSION "${FlatBuffers_VERSION}"
        SUPPLIER "Organization: Google LLC"
        DOWNLOAD_LOCATION https://github.com/google/flatbuffers
        LICENSE "Apache-2.0"
        EXTREF "cpe:2.3:a:google:flatbuffers:${FlatBuffers_VERSION}:*:*:*:*:*:*:*"
        RELATIONSHIP "@SBOM_LAST_SPDXID@ BUILD_TOOL_OF SPDXRef-Package-${PROJECT_NAME}"
    )
endif()

if(ZLIB_VERSION_STRING)
    sbom_add(PACKAGE ZLIB
        VERSION "${ZLIB_VERSION_STRING}"
        SUPPLIER "Organization: Jean-loup Gailly and Mark Adler"
        DOWNLOAD_LOCATION https://zlib.net
        LICENSE "Zlib"
        EXTREF "cpe:2.3:a:zlib:zlib:${ZLIB_VERSION_STRING}:*:*:*:*:*:*:*"
        RELATIONSHIP "${_app_spdxid}"
    )
endif()

if(Python_FOUND)
    sbom_add(PACKAGE Python
        VERSION "${Python_VERSION}"
        SUPPLIER "Organization: Python Software Foundation"
        DOWNLOAD_LOCATION https://www.python.org
        LICENSE "PSF-2.0"
        EXTREF "cpe:2.3:a:python:python:${Python_VERSION}:*:*:*:*:*:*:*"
        RELATIONSHIP "@SBOM_LAST_SPDXID@ BUILD_TOOL_OF SPDXRef-Package-${PROJECT_NAME}"
    )
endif()

sbom_add(PACKAGE eu-lotl
    VERSION "NOASSERTION"
    SUPPLIER "Organization: European Commission"
    DOWNLOAD_LOCATION "${TSL_URL}"
    LICENSE "LicenseRef-EC-open-data"
    RELATIONSHIP "${_app_spdxid}"
)

if("EE" IN_LIST TSL_INCLUDE)
    sbom_add(PACKAGE EE-tsl
        VERSION "NOASSERTION"
        SUPPLIER "Organization: Estonian Information System Authority"
        DOWNLOAD_LOCATION "https://sr.riik.ee/tsl/estonian-tsl.xml"
        LICENSE "LicenseRef-EC-open-data"
        RELATIONSHIP "${_app_spdxid}"
    )
endif()

sbom_add(PACKAGE cdoc-schema
    VERSION "NOASSERTION"
    SUPPLIER "Organization: Open Electronic Identity"
    DOWNLOAD_LOCATION https://github.com/open-eid/libcdoc
    LICENSE "MIT"
    RELATIONSHIP "${_app_spdxid}"
)


sbom_add(PACKAGE qtsingleapplication
    VERSION "NOASSERTION"
    SUPPLIER "Organization: The Qt Company"
    DOWNLOAD_LOCATION https://github.com/open-eid/qt-common
    LICENSE "BSD-3-Clause"
    RELATIONSHIP "${_app_spdxid}"
)

sbom_add(PACKAGE pkcs11.h
    VERSION "NOASSERTION"
    SUPPLIER "Organization: g10 Code GmbH"
    DOWNLOAD_LOCATION https://www.gnupg.org/software/libgcrypt/
    LICENSE "LicenseRef-unlimited-permission"
    RELATIONSHIP "${_app_spdxid}"
)

sbom_add(PACKAGE Roboto
    VERSION "NOASSERTION"
    SUPPLIER "Organization: Google LLC"
    DOWNLOAD_LOCATION https://fonts.google.com/specimen/Roboto
    LICENSE "Apache-2.0"
    RELATIONSHIP "${_app_spdxid}"
)

sbom_finalize(NO_VERIFY)
