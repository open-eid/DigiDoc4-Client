# DigiDoc4 Client

 * License: LGPL 2.1
 * &copy; Estonian Information System Authority
 * [Architecture of ID-software](http://open-eid.github.io)

Client is actively developed and is currently in alpha-stage.

## Building
[![Build Status](https://travis-ci.org/open-eid/DigiDoc4-Client.svg?branch=master)](https://travis-ci.org/open-eid/DigiDoc4-Client)
* [Ubuntu](#ubuntu)
* [macOS](#macos)

### Ubuntu

1. Install dependencies (libdigidocpp-dev must be installed from RIA repository)
   * Add custom RIA repository to APT repository list

         curl https://installer.id.ee/media/install-scripts/ria-public.key | sudo apt-key add -
         curl https://installer.id.ee/media/install-scripts/C6C83D68.pub | sudo apt-key add -
         sudo echo "deb http://installer.id.ee/media/ubuntu/ $(lsb_release -sc) main" > /etc/apt/sources.list.d/repo.list
         sudo apt-get update

   * Install

         sudo apt-get install cmake qttools5-dev libqt5svg5-dev qttools5-dev-tools libpcsclite-dev libssl-dev libdigidocpp-dev libldap2-dev

2. Fetch the source

        git clone --recursive https://github.com/open-eid/qdigidoc
        cd qdigidoc

3. Configure

        mkdir build
        cd build
        cmake ..

4. Build

        make

5. Install

        sudo make install

6. Execute

        /usr/local/bin/qdigidocclient

### macOS

1. Install dependencies from
   * [XCode](https://itunes.apple.com/en/app/xcode/id497799835?mt=12)
   * [http://www.cmake.org](http://www.cmake.org)
   * [http://qt-project.org](http://qt-project.org)
       Since Qt 5.6 default SSL backend is SecureTransport and this project depends on openssl.
       See how to build [OSX Qt from source](#building-osx-qt-from-source).
       
       Alternatively build Qt with openssl backend using provided [prepare_osx_build_environment.sh](prepare_osx_build_environment.sh) script.
   * [libdigidocpp-*.pkg](https://github.com/open-eid/libdigidocpp/releases)

2. Fetch the source

        git clone --recursive https://github.com/open-eid/DigiDoc4-Client
        cd DigiDoc4-Client

3. Configure

        mkdir build
        cd build
        cmake -DQt5_DIR="~/cmake_builds/Qt-5.9.1-OpenSSL/lib/cmake/Qt5" -DCMAKE_EXE_LINKER_FLAGS="-F/Library/Frameworks" ..

4. Build

        make

5. Install

        sudo make install

6. Execute

        open /usr/local/bin/qdigidoc4.app

