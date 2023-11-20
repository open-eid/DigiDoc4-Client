# DigiDoc4 Client

![European Regional Development Fund](client/images/EL_Regionaalarengu_Fond.png "European Regional Development Fund - DO NOT REMOVE THIS IMAGE BEFORE 05.03.2020")

 * License: LGPL 2.1
 * &copy; Estonian Information System Authority
 * [Architecture of ID-software](http://open-eid.github.io)

## Building
[![Build Status](https://github.com/open-eid/DigiDoc4-Client/workflows/CI/badge.svg?branch=master)](https://github.com/open-eid/DigiDoc4-Client/actions)
* [Ubuntu](#ubuntu)
* [macOS](#macos)
* [Windows](#windows)

### Ubuntu

1. Install dependencies (libdigidocpp-dev must be installed from RIA repository)
   * Add custom RIA repository to APT repository list

         curl https://installer.id.ee/media/install-scripts/C6C83D68.pub | gpg --dearmor | tee /etc/apt/trusted.gpg.d/ria-repository.gpg > /dev/null
         echo "deb http://installer.id.ee/media/ubuntu/ $(lsb_release -sc) main" | sudo tee /etc/apt/sources.list.d/ria-repository.list
         sudo apt update

   * Install

         # Ubuntu
         sudo apt install cmake qt6-tools-dev libqt6core5compat6-dev libqt6svg6-dev libpcsclite-dev libssl-dev libdigidocpp-dev libldap2-dev gettext pkg-config  libflatbuffers-dev zlib1g-dev
         # Fedora
         sudo dnf install qt6-qtsvg-devel qt6-qttools-devel qt6-qt5compat-devel pcsc-lite-devel openssl-devel libdigidocpp openldap-devel gettext pkg-config flatbuffers-devel flatbuffers-compiler

   * Also runtime dependency opensc-pkcs11 and pcscd is needed

2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        cmake -B build -S .

4. Build

        cmake --build build

5. Execute

        ./build/client/qdigidoc4

### macOS

1. Install dependencies from
   * [XCode](https://apps.apple.com/us/app/xcode/id497799835?mt=12)
   * [http://www.cmake.org](http://www.cmake.org)
   * [http://qt-project.org](http://qt-project.org)  
       Build universal binary of Qt using provided [prepare_osx_build_environment.sh](prepare_osx_build_environment.sh) script; by default Qt is built in the `~/cmake_builds` folder but alternate build path can be defined with the `-p` option.
   * [libdigidocpp-*.pkg](https://github.com/open-eid/libdigidocpp/releases)

2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        cmake -B build -S . \
          -DCMAKE_PREFIX_PATH=~/cmake_builds/Qt-6.5.3-OpenSSL
          -DOPENSSL_ROOT_DIR=~/cmake_build/OpenSSL \
          -DLDAP_ROOT=~/cmake_build/OpenLDAP \
          -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

4. Build

        cmake --build build

5. Execute

        open build/client/qdigidoc4.app


### Windows

1. Install dependencies from
    * [Visual Studio Community 2019](https://www.visualstudio.com/downloads/)
    * [http://www.cmake.org](http://www.cmake.org)
    * [http://qt-project.org](http://qt-project.org)
    * [libdigidocpp-*.msi](https://github.com/open-eid/libdigidocpp/releases)

2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        cmake -G"NMAKE Makefiles" -DCMAKE_PREFIX_PATH=C:\Qt\6.5.3\msvc2019_x64  -DLibDigiDocpp_ROOT="C:\Program Files (x86)\libdigidocpp" -B build -S .

4. Build

        cmake --build build

6. Execute

        build\client\qdigidoc4.exe


## Support
Official builds are provided through official distribution point [id.ee](https://www.id.ee/en/article/install-id-software/). If you want support, you need to be using official builds. Contact our support via www.id.ee for assistance.

Source code is provided on "as is" terms with no warranty (see license for more information). Do not file Github issues with generic support requests.
