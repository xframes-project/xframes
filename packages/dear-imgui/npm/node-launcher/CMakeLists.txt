cmake_minimum_required(VERSION 3.10)
project(NodeEmbeddingExample)

# Set the C++ standard to C++17 (or later)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to the Node.js source directory (it must be built already)
set(NODEJS_PATH "../../cpp/deps/node")

# Set the path to the V8 headers and libraries
set(V8_INCLUDE_DIR "${NODEJS_PATH}/deps/v8/include")
set(V8_LIB_DIR "${NODEJS_PATH}/out/Release/lib")

# Set the paths for libplatform and libuv if needed (Node.js includes them)
set(LIBUV_INCLUDE_DIR "${NODEJS_PATH}/deps/uv/include")
set(LIBUV_LIB_DIR "${NODEJS_PATH}/out/Release/obj.target/deps/uv/src")

# Include the necessary headers
include_directories(
        "${V8_INCLUDE_DIR}"
        "${LIBUV_INCLUDE_DIR}"
        "${NODEJS_PATH}/src"
)

# Add the source files
add_executable(node_embed src/main.cpp)

# Link against Node.js, V8, and libuv libraries
target_link_libraries(node_embed
        "${V8_LIB_DIR}/v8_libplatform"
#        "${LIBUV_LIB_DIR}/libuv.lib"     # Path to libuv library
)

# Link against necessary system libraries
target_link_libraries(node_embed
        kernel32.lib
        user32.lib
        gdi32.lib
        winspool.lib
        comdlg32.lib
        advapi32.lib
)