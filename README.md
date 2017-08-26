# DigiDoc3 Client

 * License: LGPL 2.1
 * &copy; Estonian Information System Authority
 * [Architecture of ID-software](http://open-eid.github.io)

## Building
[![Build Status](https://travis-ci.org/open-eid/qdigidoc.svg?branch=master)](https://travis-ci.org/open-eid/qdigidoc)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/open-eid/qdigidoc?branch=master&svg=true)](https://ci.appveyor.com/project/open-eid/qdigidoc)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/725/badge.svg)](https://scan.coverity.com/projects/725)
* [Ubuntu](#ubuntu)
* [OS X](#osx)
* [Windows](#windows)

### Ubuntu

1. Install dependencies

        sudo apt-get install cmake qttools5-dev qttools5-dev-tools libpcsclite-dev libssl-dev libdigidocpp-dev libldap2-dev

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
        
### OSX

1. Install dependencies from
   * [XCode](https://itunes.apple.com/en/app/xcode/id497799835?mt=12)
   * [http://www.cmake.org](http://www.cmake.org)
   * [http://qt-project.org](http://qt-project.org)
       Since Qt 5.6 default SSL backend is SecureTransport and this project depends openssl.
       See how to build [OSX Qt from source](#building-osx-qt-from-source)
   * [libdigidocpp-*.pkg](https://github.com/open-eid/libdigidocpp/releases)
2. Fetch the source

        git clone --recursive https://github.com/open-eid/qdigidoc
        cd qdigidoc

3. Configure

        mkdir build
        cd build
        cmake -DQt5_DIR="~/Qt/5.5/clang_64/lib/cmake/Qt5" ..

4. Build

        make

5. Install

        sudo make install

6. Execute

        open /usr/local/bin/qdigidocclient.app

#### Building OSX Qt from source

    brew install openssl

    curl -O -L http://download.qt.io/official_releases/qt/5.8/5.8.0/submodules/qtbase-opensource-src-5.8.0.tar.gz
    tar xf qtbase-opensource-src-5.8.0.tar.gz
    cd qtbase-opensource-src-5.8.0
    ./configure -prefix /Developer/Qt-5.8.0 -opensource -nomake tests -nomake examples -no-securetransport -openssl-runtime -confirm-license -I /usr/local/opt/openssl/include -L /usr/local/opt/openssl/lib
    make
    sudo make install
    cd ..
    rm -rf qtbase-opensource-src-5.8.0

    curl -O -L http://download.qt.io/official_releases/qt/5.8/5.8.0/submodules/qttools-opensource-src-5.8.0.tar.gz
    tar xf qttools-opensource-src-5.8.0.tar.gz
    cd qttools-opensource-src-5.8.0
    /Developer/Qt-5.8.0/bin/qmake
    make
    sudo make install
    cd ..
    rm -rf qttools-opensource-src-5.8.0

### Windows

1. Install dependencies from
    * [Visual Studio Express 2013 for Windows Desktop](http://www.visualstudio.com/en-us/products/visual-studio-express-vs.aspx)
    * [http://www.cmake.org](http://www.cmake.org)
    * [http://qt-project.org](http://qt-project.org)
    * [libdigidocpp-*.msi](https://github.com/open-eid/libdigidocpp/releases)
2. Fetch the source

        git clone --recursive https://github.com/open-eid/qdigidoc
        cd qdigidoc

3. Configure

        mkdir build
        cd build
        cmake -G"NMAKE Makefiles" -DQt5_DIR="C:\Qt\5.6\msvc2013\lib\cmake\Qt5" ..

4. Build

        nmake

6. Execute

        client\qdigidocclient.exe



## Support
Official builds are provided through official distribution point [installer.id.ee](https://installer.id.ee). If you want support, you need to be using official builds. Contact for assistance by email [abi@id.ee](mailto:abi@id.ee) or [www.id.ee](http://www.id.ee).

Source code is provided on "as is" terms with no warranty (see license for more information). Do not file Github issues with generic support requests.
