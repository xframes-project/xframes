set CMAKE_TOOLCHAIN_FILE=C:/dev/react-imgui/packages/dear-imgui/cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake
set CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=C:/dev/react-imgui/packages/dear-imgui/cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake"
pipx run build
pip install .