cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(VCPKG_TARGET_TRIPLET "x64-windows")

project(reactDearImgui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)
find_package(imgui REQUIRED)
find_package(Stb REQUIRED)
#find_package(ada CONFIG REQUIRED)
#find_package(JPEG REQUIRED)
#find_package(PNG REQUIRED)
#find_package(TIFF REQUIRED)
#find_package(httplib CONFIG REQUIRED)
#find_package(OpenSSL REQUIRED)

add_executable(openglTest
        main.cpp
)

target_include_directories(
        openglTest PRIVATE

        ${Stb_INCLUDE_DIR}
)

target_link_libraries(openglTest PRIVATE
        imgui::imgui
#        ada::ada
#        JPEG::JPEG
#        PNG::PNG
#        TIFF::TIFF
#        httplib::httplib
#        OpenSSL::SSL
#        OpenSSL::Crypto
#        ${CMAKE_CURRENT_SOURCE_DIR}/glfw3dll.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/opengl32.lib
)
