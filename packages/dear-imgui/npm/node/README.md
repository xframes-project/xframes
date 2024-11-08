# xframes for Node.js

## Building

### Windows

If a prebuilt module isn't found then you will need Visual Studio 2022

### WSL2 - Ubuntu 24.04

You may need to run `export GALLIUM_DRIVER=d3d12` before starting the application to enable Direct3D 12 rendering support.
This setting is required for proper GPU acceleration in WSL2 and needs to be set in each new terminal session.
You can add this line to your ~/.bashrc to make it permanent.

### Linux

If a prebuilt module isn't found then you will need gcc 13+

Ubuntu 24.04 dependencies:

`sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config`