# embedded Node.js wrapper

## Notes

Investigate how https://github.com/jet2jet/pe-library-js might help ensure the application runs without opening a terminal first.
"win32metadata": {"Subsystem": "windows"}

vcbuild.bat nosign release x64 static
./configure --fully-static --enable-static

https://learn.microsoft.com/en-gb/windows-hardware/drivers/download-the-wdk

https://v8.dev/docs/build-gn

gclient config https://chromium.googlesource.com/v8/v8.git

set DEPOT_TOOLS_WIN_TOOLCHAIN=0

set PATH=%PATH%;C:\u-blox\gallery\ubx\ulogr\nasm;C:\u-blox\gallery\ubx\ulogr\gn;C:\u-blox\gallery\ubx\ulogr\depot_tools

export PATH=$PATH:"/c/u-blox/gallery/ubx/ulogr/nasm":"/c/u-blox/gallery/ubx/ulogr/gn":"/c/u-blox/gallery/ubx/ulogr/depot_tools"

gn gen out/Release --args="is_component_build=false is_debug=false v8_monolithic=true v8_use_external_startup_data=false target_cpu=\"x64\""
ninja -C out/Release