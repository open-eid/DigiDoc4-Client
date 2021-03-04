#!/bin/bash
# Requires xcode and cmake, also OpenSSL has to be installed via homebrew.

set -e

######### Versions of libraries/frameworks to be compiled
QT_VER="5.12.10"
OPENLDAP_VER="2.4.56"
REBUILD=false
BUILD_PATH=~/cmake_builds
OPENSSL_PATH="/usr/local/opt/openssl"
: ${MACOSX_DEPLOYMENT_TARGET:="10.13"}
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
    echo -e "\n${ORANGE}##### Building Qt ${QT_VER} ${QT_PATH} #####${RESET}\n"
    mkdir -p ${BUILD_PATH} && cd ${BUILD_PATH}
    for ARCH in x86_64 arm64; do
        for PACKAGE in qtbase-everywhere-src-${QT_VER} qtsvg-everywhere-src-${QT_VER} qttools-everywhere-src-${QT_VER}; do
            if [ ! -f ${PACKAGE}.tar.xz ]; then
                curl -O -L http://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/${PACKAGE}.tar.xz
            fi
            rm -rf ${PACKAGE}
            tar xf ${PACKAGE}.tar.xz
            pushd ${PACKAGE}
            if [[ "${PACKAGE}" == *"qtbase"* ]] ; then
                if [[ "${ARCH}" == "arm64" ]] ; then
                    CROSSCOMPILE="-device-option QMAKE_APPLE_DEVICE_ARCHS=${ARCH}"
                fi
                ./configure -prefix ${QT_PATH} -opensource -nomake tests -nomake examples -no-securetransport -openssl -confirm-license OPENSSL_PREFIX=${OPENSSL_PATH} ${CROSSCOMPILE}
            else
                "${QT_PATH}"/bin/qmake
            fi
            make
            make install
            popd
            rm -rf ${PACKAGE}
        done
        sudo mv ${QT_PATH} ${QT_PATH}.${ARCH}
    done
    sudo cp -a ${QT_PATH}.x86_64 ${QT_PATH}
    pushd ${QT_PATH}.arm64
        for i in lib/Qt*.framework/Versions/Current/Qt* plugins/*/*.dylib; do
            sudo lipo -create ${QT_PATH}.x86_64/${i} ${i} -output ${QT_PATH}/${i};
        done
    popd
else
    echo -e "\n${GREY}  Qt not built${RESET}"
fi

if [[ "$REBUILD" = true || ! -d ${OPENLDAP_PATH} ]] ; then
    SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
    echo -e "\n${ORANGE}##### Building OpenLDAP ${OPENLDAP_VER} ${OPENLDAP_PATH} #####${RESET}\n"
    curl -O -L http://mirror.eu.oneandone.net/software/openldap/openldap-release/openldap-${OPENLDAP_VER}.tgz
    tar xf openldap-${OPENLDAP_VER}.tgz
    cd openldap-${OPENLDAP_VER}
    patch -Np1 -i ${SCRIPTPATH}/openldap.patch
    ARCH="-arch x86_64 -arch arm64"
    CFLAGS="${ARCH}" CXXFLAGS="${ARCH}" LDFLAGS="${ARCH} -L${OPENSSL_PATH}/lib" CPPFLAGS="-I${OPENSSL_PATH}/include" ./configure -prefix ${OPENLDAP_PATH} \
        --enable-static --disable-shared --disable-syslog --disable-proctitle --disable-local --disable-slapd \
        --without-threads --without-cyrus-sasl --with-tls=openssl
    make
    make install
else
    echo -e "\n${GREY}  OpenLDAP not built${RESET}"
fi
