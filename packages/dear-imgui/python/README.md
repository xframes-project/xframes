SKBUILD_BUILD_DIR: ""build/{wheel_tag}""
set CMAKE_TOOLCHAIN_FILE=C:/dev/react-imgui/packages/dear-imgui/cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake
set CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=C:/dev/react-imgui/packages/dear-imgui/cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake"
pipx run build
pip install .

set CMAKE_CXX_FLAGS=/WX-
set CMAKE_CXX_FLAGS_DEBUG=/WX-
set CMAKE_CXX_FLAGS_RELEASE=/WX-
