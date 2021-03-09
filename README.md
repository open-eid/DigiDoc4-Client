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

         curl https://installer.id.ee/media/install-scripts/C6C83D68.pub | sudo apt-key add -
         sudo echo "deb http://installer.id.ee/media/ubuntu/ $(lsb_release -sc) main" > /etc/apt/sources.list.d/repo.list
         sudo apt-get update

   * Install

         sudo apt-get install cmake qttools5-dev libqt5svg5-dev qttools5-dev-tools libpcsclite-dev libssl-dev libdigidocpp-dev libldap2-dev gettext pkg-config

   * Also runtime dependency opensc-pkcs11 and pcscd is needed

2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        mkdir build
        cd build
        cmake ..

4. Build

        make

5. Install

        sudo make install

6. Execute

        /usr/local/bin/qdigidoc4

### macOS

1. Install dependencies from
   * [XCode](https://apps.apple.com/us/app/xcode/id497799835?mt=12)
   * [http://www.cmake.org](http://www.cmake.org)
   * [http://qt-project.org](http://qt-project.org)
       Since Qt 5.6 default SSL backend is SecureTransport and this project depends on openssl.
       See how to build [OSX Qt from source](#building-osx-qt-from-source).
       
       Alternatively build Qt with openssl backend using provided [prepare_osx_build_environment.sh](prepare_osx_build_environment.sh) script; by default Qt is built in the `~/cmake_builds` folder but alternate build path can be defined with the `-p` option.
   * [libdigidocpp-*.pkg](https://github.com/open-eid/libdigidocpp/releases)

2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        mkdir build
        cd build
        cmake -DQt5_DIR="~/cmake_builds/Qt-5.12.10-OpenSSL/lib/cmake/Qt5" -DCMAKE_EXE_LINKER_FLAGS="-F/Library/Frameworks" ..

4. Build

        make

5. Install

        sudo make install

6. Execute

        open /usr/local/bin/qdigidoc4.app


### Windows

1. Install dependencies from
    * [Visual Studio Community 2017](https://www.visualstudio.com/downloads/)
    * [http://www.cmake.org](http://www.cmake.org)
    * [http://qt-project.org](http://qt-project.org)
    * [libdigidocpp-*.msi](https://github.com/open-eid/libdigidocpp/releases)
2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        mkdir build
        cd build
        cmake -G"NMAKE Makefiles" -DQt5_DIR="C:\Qt\5.12.10\msvc2017\lib\cmake\Qt5" ..

4. Build

        nmake

6. Execute

        client\qdigidoc4.exe


## Support
Official builds are provided through official distribution point [id.ee](https://www.id.ee/en/article/install-id-software/). If you want support, you need to be using official builds. Contact our support via www.id.ee for assistance.

Source code is provided on "as is" terms with no warranty (see license for more information). Do not file Github issues with generic support requests.
