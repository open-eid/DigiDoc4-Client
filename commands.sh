# clean up the previous build
sudo rm -rf ./build

# build a Docker image with dev depencencies. A DigiDoc4 client will be built using this image
docker build -f ./Dockerfile.builder -t tasmanianfox/digidoc4:builder .

# Build the app in Docker container
mkdir ./build
docker run --rm -u "$(id -u)" -v $PWD:/app -it tasmanianfox/digidoc4:builder cmake ..
docker run --rm -u "$(id -u)" -v $PWD:/app -it tasmanianfox/digidoc4:builder make
docker run --rm  -u "$(id -u)" -v $PWD:/app -it tasmanianfox/digidoc4:builder make install DESTDIR=AppDir

# Copy icons to the directory where appimage-builder expects them
mkdir ./build/AppDir/usr/share
cp -R ./build/AppDir/usr/local/share/icons ./build/AppDir/usr/share/icons

# Just a sample. This is how AppImageBuilder.yml is generated, BUT there are additional chenges done in this file (e.g. installation of digidoc libraries)
# docker run --rm -u "$(id -u)" -v $PWD/build:/build -w /build -it appimagecrafters/appimage-builder:latest appimage-builder --generate

# Build an AppImage. The final image will be saved to "build/DigiDoc4 Client-latest-x86_64.AppImage"
docker run --rm -u "$(id -u)" -v $PWD/build:/build -w /build -it appimagecrafters/appimage-builder:latest appimage-builder --recipe /build/AppImageBuilder.yml --skip-test