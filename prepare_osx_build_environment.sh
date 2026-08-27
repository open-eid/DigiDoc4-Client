#!/bin/bash
# Requires xcode and cmake.

set -e

######### Versions of libraries/frameworks to be compiled
QT_VER="6.10.3"
OPENSSL_VER="3.5.7"
OPENLDAP_VER="2.6.14"
REBUILD=false
BUILD_PATH=~/cmake_builds
: ${MACOSX_DEPLOYMENT_TARGET:="14.0"}
export MACOSX_DEPLOYMENT_TARGET

######### Checksums for the versions above; update alongside any version bump
OPENSSL_SHA256="a8c0d28a529ca480f9f36cf5792e2cd21984552a3c8e4aa11a24aa31aeac98e8"
QTBASE_SHA256="383dc907816338f0cba72088a524c07458dfc69ce684ca9132fcc4fe91c24b0b"
QTSVG_SHA256="b3223fe005f6a4c7f21f34e4ee6ce0094737cff9503ba15bbb171ac18794f76d"
QTTOOLS_SHA256="8f00b9e3d1f80973d81cff67684972b89993183ef19924404d5b8ff0f89675b6"
OPENLDAP_SHA256="806dcd21d366428187fba3278da773d5930f774852c9e92517f950d585f19107"

verify_sha256() {
    local file="$1" expected="$2" actual
    actual=$(shasum -a 256 "${file}" | cut -d' ' -f1)
    if [[ "${actual}" != "${expected}" ]]; then
        echo -e "${RED}Checksum mismatch for ${file}: expected ${expected}, got ${actual}${RESET}"
        exit 1
    fi
}

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
        : ${OPENSSL_PATH:="${BUILD_PATH}/OpenSSL"}
        shift
        ;;
    -r|--rebuild)
        REBUILD=true
        ;;
    -h|--help)
        echo "Build Qt for Digidoc4 client"
        echo ""
        echo "Usage: $0 [-r|--rebuild] [-p|--path build-path] [-o|--openssl openssl-path] [-h|--help]"
        echo ""
        echo "Options:"
        echo "  -o or --openssl openssl-path:"
        echo "     OpenSSL path; default ${OPENSSL_VER} will be built ${OPENSSL_PATH}"
        echo "  -p or --path build-path"
        echo "     folder where the dependencies should be built; default ${BUILD_PATH}"
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

mkdir -p ${BUILD_PATH}
pushd ${BUILD_PATH}

if [[ ! -d ${OPENSSL_PATH} ]] ; then
    echo -e "\n${ORANGE}##### Building OpenSSL ${OPENSSL_VER} ${OPENSSL_PATH} #####${RESET}\n"
    if [ ! -f openssl-${OPENSSL_VER}.tar.gz ]; then
        curl -O -L --proto '=https' --proto-redir '=https' https://www.openssl.org/source/openssl-${OPENSSL_VER}.tar.gz
    fi
    verify_sha256 openssl-${OPENSSL_VER}.tar.gz ${OPENSSL_SHA256}
    rm -rf openssl-${OPENSSL_VER}
    tar xf openssl-${OPENSSL_VER}.tar.gz
    pushd openssl-${OPENSSL_VER}
    for ARCH in x86_64 arm64; do
        ./Configure darwin64-${ARCH} --prefix=${OPENSSL_PATH} no-apps shared no-autoload-config no-module no-tests enable-ec_nistp_64_gcc_128
        make -s > /dev/null
        make install_sw
        mv ${OPENSSL_PATH}{,.${ARCH}}
        make distclean
    done
    popd
    cp -a ${OPENSSL_PATH}{.x86_64,}
    pushd ${OPENSSL_PATH}.arm64
    for i in lib/lib*3.dylib; do
        lipo -create ${OPENSSL_PATH}.x86_64/${i} ${i} -output ${OPENSSL_PATH}/${i}
    done
    popd
else
    echo -e "\n${GREY}  OpenSSL not built${RESET}"
fi

if [[ "$REBUILD" = true || ! -d ${QT_PATH} ]] ; then
    qt_ver_parts=( ${QT_VER//./ } )
    QT_MINOR="${qt_ver_parts[0]}.${qt_ver_parts[1]}"
    echo -e "\n${ORANGE}##### Building Qt ${QT_VER} ${QT_PATH} #####${RESET}\n"
    for PACKAGE in qtbase-everywhere-src-${QT_VER} qtsvg-everywhere-src-${QT_VER} qttools-everywhere-src-${QT_VER}; do
        if [ ! -f ${PACKAGE}.tar.xz ]; then
            curl -O -L --proto '=https' --proto-redir '=https' https://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/${PACKAGE}.tar.xz
        fi
        case "${PACKAGE}" in
            qtbase-*) verify_sha256 ${PACKAGE}.tar.xz ${QTBASE_SHA256} ;;
            qtsvg-*) verify_sha256 ${PACKAGE}.tar.xz ${QTSVG_SHA256} ;;
            qttools-*) verify_sha256 ${PACKAGE}.tar.xz ${QTTOOLS_SHA256} ;;
        esac
        rm -rf ${PACKAGE}
        tar xf ${PACKAGE}.tar.xz
        pushd ${PACKAGE}
        if [[ "${PACKAGE}" == *"qtbase"* ]] ; then
            ./configure -prefix ${QT_PATH} -opensource -sbom -appstore-compliant -confirm-license -openssl-linked \
                -no-securetransport -nomake tests -nomake examples -no-dbus -no-libjpeg -no-gif -- \
                -DOPENSSL_ROOT_DIR=${OPENSSL_PATH} -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
        else
            ${QT_PATH}/bin/qt-configure-module .
        fi
        cmake --build . --parallel
        cmake --install .
        popd
        rm -rf ${PACKAGE}
    done
else
    echo -e "\n${GREY}  Qt not built${RESET}"
fi

if [[ "$REBUILD" = true || ! -d ${OPENLDAP_PATH} ]] ; then
    echo -e "\n${ORANGE}##### Building OpenLDAP ${OPENLDAP_VER} ${OPENLDAP_PATH} #####${RESET}\n"
    if [ ! -f openldap-${OPENLDAP_VER}.tgz ]; then
        curl -O -L --proto '=https' --proto-redir '=https' https://www.openldap.org/software/download/OpenLDAP/openldap-release/openldap-${OPENLDAP_VER}.tgz
    fi
    verify_sha256 openldap-${OPENLDAP_VER}.tgz ${OPENLDAP_SHA256}
    tar xf openldap-${OPENLDAP_VER}.tgz
    pushd openldap-${OPENLDAP_VER}
    sed -ie 's! doc!!' Makefile.in
    ARCH="-arch x86_64 -arch arm64"
    CFLAGS="${ARCH}" CXXFLAGS="${ARCH}" LDFLAGS="${ARCH} -L${OPENSSL_PATH}/lib" CPPFLAGS="-I${OPENSSL_PATH}/include" ./configure \
        --prefix ${OPENLDAP_PATH} --enable-static --disable-shared --disable-syslog --disable-local --disable-slapd \
        --without-threads --without-cyrus-sasl --with-tls=openssl
    make
    make install
    popd
else
    echo -e "\n${GREY}  OpenLDAP not built${RESET}"
fi

popd
