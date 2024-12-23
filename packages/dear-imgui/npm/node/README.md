# xframes for Node.js

## Building

### Windows

If a prebuilt module isn't found then you will need Visual Studio 2022

### WSL2 - Ubuntu 24.04

You may need to run `export GALLIUM_DRIVER=d3d12` before starting the application to enable Direct3D 12 rendering support.
This setting is required for proper GPU acceleration in WSL2 and needs to be set in each new terminal session.
You can add this line to your ~/.bashrc to make it permanent.

### Linux

If a prebuilt module isn't found then you will need gcc 13+ to build the project locally.

Ubuntu 24.04 dependencies:

`sudo apt install curl zip unzip tar build-essential cmake libglfw3 libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config`

Fedora 41 dependencies:

`sudo dnf install @development-tools gcc-c++ cmake glfw-devel`

Raspberry Pi OS

`sudo apt install curl zip unzip tar build-essential cmake libglfw3 libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config`

You must set:

`export ARM64_LINUX=1`

We could not work out how to detect the correct architecture so, for convenience, in CMakeLists.txt we check for this ENV variable to be set.

`export VCPKG_FORCE_SYSTEM_BINARIES=1`

It suppresses the downloading of CMake and Ninja and forces the use of the system binaries.