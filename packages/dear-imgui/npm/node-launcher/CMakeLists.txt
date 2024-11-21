cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

if(NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "Ninja")
endif()

set(CMAKE_TOOLCHAIN_FILE ../../cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

if(NOT VCPKG_TARGET_TRIPLET)
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows")
    elseif(APPLE)
        set(VCPKG_TARGET_TRIPLET "x64-osx")
    elseif(DEFINED ENV{ARM64_LINUX})
        set(VCPKG_TARGET_TRIPLET "arm64-linux")
    else()
        set(VCPKG_TARGET_TRIPLET "x64-linux")
    endif()
endif()

project (xframes-node-launcher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

set(NODE_SRC_PATH "${CMAKE_SOURCE_DIR}/../../cpp/deps/node")
set(NODE_LIB_PATH "${NODE_SRC_PATH}/out/Release/lib")

# Add Node.js include directory
#include_directories(${NODE_SRC_PATH})
include_directories(${NODE_SRC_PATH} ${NODE_SRC_PATH}/src ${NODE_SRC_PATH}/deps/v8/include ${NODE_SRC_PATH}/deps/uv/include)

# Add the source directory of Node.js build
link_directories(${NODE_LIB_PATH})

# Specify the sources of your project
add_executable(${PROJECT_NAME} src/main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME} PRIVATE
        ${NODE_LIB_PATH}/../node.lib
        ${NODE_LIB_PATH}/v8_libplatform.lib
#        ${NODE_LIB_PATH}/uv.lib # Include libuv if needed
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${NODE_SRC_PATH}/src
        ${NODE_SRC_PATH}/deps/v8/include
        ${NODE_SRC_PATH}/deps/uv/include
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
