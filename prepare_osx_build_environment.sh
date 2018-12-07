#!/bin/bash
# Requires xcode and cmake, also OpenSSL has to be installed via homebrew.

######### Versions of libraries/frameworks to be compiled
QT_VER="5.9.7"
CMAKE_OSX_DEPLOYMENT_TARGET="10.10.5"
#########


set -e

REBUILD=false
BUILD_PATH=~/cmake_builds
PROG=$0
TARGETS=""

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
    -r|--rebuild)
        REBUILD=true
        ;;
    -h|--help)
        echo "Build Qt for Digidoc4 client"
        echo ""
        echo "Usage: $PROG [-r|--rebuild] [-p build-path] [-h|--help] [-q|--qt qt-version]"
        echo ""
        echo "Options:"
        echo "  -o or --openssl openssl-path:"
        echo "     OpenSSL path; default found from homebrew"
        echo "  -p or --path build-path"
        echo "     folder where the dependencies should be built; default ~/cmake_builds"
        echo "  -q or --qt qt-version:"
        echo "     Specific version of Qt to build; default $QT_VER "
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

if [ -z "$OPENSSL_PATH" ] ; then
    OPENSSL_PATH=`ls -d /usr/local/Cellar/openssl/1.0.* | tail -n 1`
fi
qt_ver_parts=( ${QT_VER//./ } )
QT_MINOR="${qt_ver_parts[0]}.${qt_ver_parts[1]}"
QT_PATH=${BUILD_PATH}/Qt-${QT_VER}-OpenSSL
QT_SRCSPEC="opensource"
if [[ "$QT_MINOR" = "5.10" ]] ; then
    QT_SRCSPEC="everywhere"
fi

GREY='\033[0;37m'
ORANGE='\033[0;33m'
RED='\033[0;31m'
RESET='\033[0m'

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
SCRIPT_PATH=$(dirname $(realpath "$0") )


if [[ "$REBUILD" = true || ! -d ${QT_PATH} ]] ; then
    echo -e "\n${ORANGE}##### Building Qt #####${RESET}\n"
    mkdir -p ${BUILD_PATH} && cd ${BUILD_PATH}
    curl -O -L http://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/qtbase-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    tar xf qtbase-${QT_SRCSPEC}-src-${QT_VER}.tar.xz
    cd qtbase-${QT_SRCSPEC}-src-${QT_VER}
    ./configure -prefix ${QT_PATH} -opensource -nomake tests -nomake examples -no-securetransport -openssl-linked -confirm-license OPENSSL_PREFIX=${OPENSSL_PATH}
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
