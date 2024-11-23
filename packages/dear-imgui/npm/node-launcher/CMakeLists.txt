cmake_minimum_required(VERSION 3.10)
project(NodeEmbeddingExample)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT")

set(CMAKE_VERBOSE_MAKEFILE TRUE)

set(CMAKE_GENERATOR_TOOLSET "v143")

# For Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
    # For Release builds
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# Paths
set(NODEJS_PATH "../../cpp/deps/node")
set(V8_INCLUDE_DIR "${NODEJS_PATH}/deps/v8/include")
set(V8_RELEASE_DIR "C:/dev/v8/v8/out/Release")
set(V8_LIB_DIR "${NODEJS_PATH}/out/Release/lib")
set(LIBUV_INCLUDE_DIR "${NODEJS_PATH}/deps/uv/include")

set(NODE_LIB_DIR "C:/dev/react-imgui/packages/dear-imgui/cpp/deps/node/out/Release/lib")

# Include directories
include_directories(
        "${V8_INCLUDE_DIR}"
#        "C:/dev/v8/v8/include"
        "${LIBUV_INCLUDE_DIR}"
        "${NODEJS_PATH}/src"
        "${NODEJS_PATH}/tools"
)

# Add source files
add_executable(node_embed src/main.cpp)

# "C:\dev\react-imgui\packages\dear-imgui\cpp\deps\node\out\Release\lib\libnode.lib"

# Link libraries
target_link_libraries(node_embed PRIVATE
#        "${V8_RELEASE_DIR}/d8.lib"
#        "${V8_RELEASE_DIR}/v8_sample_process.lib"
#        "${V8_RELEASE_DIR}/v8_shell.lib"
#        "${V8_RELEASE_DIR}/wami.lib"
#        "${V8_RELEASE_DIR}/obj/v8_monolith.lib"
#        "${V8_RELEASE_DIR}/obj/wee8.lib"

        $<$<CXX_COMPILER_ID:GNU>:-Wl,--start-group>


        "${NODE_LIB_DIR}/ada.lib"
        "${NODE_LIB_DIR}/brotli.lib"
        "${NODE_LIB_DIR}/cares.lib"
        "${NODE_LIB_DIR}/gtest.lib"
        "${NODE_LIB_DIR}/gtest_main.lib"
        "${NODE_LIB_DIR}/histogram.lib"
        "${NODE_LIB_DIR}/icudata.lib"
        "${NODE_LIB_DIR}/icui18n.lib"
        "${NODE_LIB_DIR}/icutools.lib"
        "${NODE_LIB_DIR}/icuucx.lib"
        "${NODE_LIB_DIR}/libnode.lib"
        "${NODE_LIB_DIR}/libuv.lib"
        "${NODE_LIB_DIR}/llhttp.lib"
        "${NODE_LIB_DIR}/nbytes.lib"
        "${NODE_LIB_DIR}/ncrypto.lib"
        "${NODE_LIB_DIR}/nghttp2.lib"
        "${NODE_LIB_DIR}/nghttp3.lib"
        "${NODE_LIB_DIR}/ngtcp2.lib"
        "${NODE_LIB_DIR}/openssl.lib"
        "${NODE_LIB_DIR}/simdjson.lib"
        "${NODE_LIB_DIR}/simdutf.lib"
        "${NODE_LIB_DIR}/sqlite.lib"
        "${NODE_LIB_DIR}/torque_base.lib"
        "${NODE_LIB_DIR}/uvwasi.lib"
        "${NODE_LIB_DIR}/v8_abseil.lib"
        "${NODE_LIB_DIR}/v8_compiler.lib"
        "${NODE_LIB_DIR}/v8_init.lib"
        "${NODE_LIB_DIR}/v8_initializers_slow.lib"
        "${NODE_LIB_DIR}/v8_libbase.lib"
        "${NODE_LIB_DIR}/v8_libplatform.lib"
        "${NODE_LIB_DIR}/v8_turboshaft.lib"
        "${NODE_LIB_DIR}/v8_zlib.lib"
        "${NODE_LIB_DIR}/zlib.lib"
        "${NODE_LIB_DIR}/zlib_adler32_simd.lib"
        "${NODE_LIB_DIR}/zlib_inflate_chunk_simd.lib"

        $<$<CXX_COMPILER_ID:GNU>:-Wl,--end-group>

        kernel32.lib
        user32.lib
        gdi32.lib
        winspool.lib
        comdlg32.lib
        advapi32.lib
        DbgHelp.lib
        Userenv.lib
        Crypt32.lib
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(node_embed PRIVATE msvcprtd.lib)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_libraries(node_embed PRIVATE msvcrt.lib)
endif()

# Output directory
set_target_properties(node_embed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)