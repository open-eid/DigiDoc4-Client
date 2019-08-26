#!/bin/bash
# Requires xcode and cmake, also OpenSSL has to be installed via homebrew.

set -e

######### Versions of libraries/frameworks to be compiled
QT_VER="5.9.8"
OPENLDAP_VER="2.4.48"
REBUILD=false
BUILD_PATH=~/cmake_builds
SCRIPT_PATH=$(cd "$(dirname "$0")"; pwd -P)
OPENSSL_PATH="/usr/local/opt/openssl"
: ${MACOSX_DEPLOYMENT_TARGET:="10.11"}
export MACOSX_DEPLOYMENT_TARGET

while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
    -o|--openssl)
        OPENSSL_PATH="$2"
        shift
        ;;
    -p|--path)
        BUILD_PATH="$2"
        shift
        ;;
    -q|--qt)
        QT_VER="$2"
        shift
        ;;
    -l|--openldap)
        OPENLDAP_VER="$2"
        shift
        ;;
    -r|--rebuild)
        REBUILD=true
        ;;
    -h|--help)
        echo "Build Qt for Digidoc4 client"
        echo ""
        echo "Usage: $0 [-r|--rebuild] [-p build-path] [-h|--help] [-q|--qt qt-version] [-l|--openldap openldap-version]"
        echo ""
        echo "Options:"
        echo "  -o or --openssl openssl-path:"
        echo "     OpenSSL path; default found from homebrew ${OPENSSL_PATH}"
        echo "  -p or --path build-path"
        echo "     folder where the dependencies should be built; default ${BUILD_PATH}"
        echo "  -q or --qt qt-version:"
        echo "     Specific version of Qt to build; default ${QT_VER} "
        echo "  -l or --openldap openldap-version:"
        echo "     Specific version of OpenLDAP to build; default ${OPENLDAP_VER} "
        echo "  -r or --rebuild:"
        echo "     Rebuild even if dependency is already built"
        echo " "
        echo "  -h or --help:"
        echo "     Print usage of the script "
        exit 0
        ;;
    esac
    shift # past argument or value
done

QT_PATH=${BUILD_PATH}/Qt-${QT_VER}-OpenSSL
OPENLDAP_PATH=${BUILD_PATH}/OpenLDAP
GREY='\033[0;37m'
ORANGE='\033[0;33m'
RED='\033[0;31m'
RESET='\033[0m'

if [[ "$REBUILD" = true || ! -d ${QT_PATH} ]] ; then
    qt_ver_parts=( ${QT_VER//./ } )
    QT_MINOR="${qt_ver_parts[0]}.${qt_ver_parts[1]}"
    QT_SRCSPEC="opensource"
    if [[ "$QT_MINOR" = "5.10" ]] ; then
        QT_SRCSPEC="everywhere"
    fi

    echo -e "\n${ORANGE}##### Building Qt ${QT_VER} ${QT_PATH} #####${RESET}\n"
    mkdir -p ${BUILD_PATH} && cd ${BUILD_PATH}
    curl -O -L http://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/qtbase-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    tar xf qtbase-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    cd qtbase-${QT_SRCSPEC}-src-${QT_VER}
    patch -Np1 -i $SCRIPT_PATH/Qt-5.9.8-OpenSSL-1.1.patch
    ./configure -prefix ${QT_PATH} -opensource -nomake tests -nomake examples -no-securetransport -openssl -confirm-license OPENSSL_PREFIX=${OPENSSL_PATH}
    make
    make install
    rm -rf ${BUILD_PATH}/qtbase-${QT_SRCSPEC}-src-${QT_VER}
    rm ${BUILD_PATH}/qtbase-${QT_SRCSPEC}-src-${QT_VER}.tar.xz

    cd ${BUILD_PATH}
    curl -O -L http://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/qtsvg-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    tar xf qtsvg-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    cd qtsvg-${QT_SRCSPEC}-src-${QT_VER}
    "${QT_PATH}"/bin/qmake
    make
    make install

    cd ${BUILD_PATH}
    curl -O -L http://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/qttools-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    tar xf qttools-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    cd qttools-${QT_SRCSPEC}-src-${QT_VER}
    "${QT_PATH}"/bin/qmake
    make
    make install
else
    echo -e "\n${GREY}  Qt not built${RESET}"
fi

if [[ "$REBUILD" = true || ! -d ${OPENLDAP_PATH} ]] ; then
    echo -e "\n${ORANGE}##### Building OpenLDAP ${OPENLDAP_VER} ${OPENLDAP_PATH} #####${RESET}\n"
    curl -O -L http://mirror.eu.oneandone.net/software/openldap/openldap-release/openldap-${OPENLDAP_VER}.tgz
    tar xf openldap-${OPENLDAP_VER}.tgz
    cd openldap-${OPENLDAP_VER}
    LDFLAGS="-L${OPENSSL_PATH}/lib" CPPFLAGS="-I${OPENSSL_PATH}/include" ./configure -prefix ${OPENLDAP_PATH} \
        --enable-static --disable-shared --disable-syslog --disable-proctitle --disable-local --disable-slapd \
        --without-threads --without-cyrus-sasl --with-tls=openssl
    make
    make install
else
    echo -e "\n${GREY}  OpenLDAP not built${RESET}"
fi
