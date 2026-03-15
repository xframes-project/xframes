cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

if(NOT VCPKG_TARGET_TRIPLET)
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows")
    elseif(APPLE)
        set(VCPKG_TARGET_TRIPLET "x64-osx")
    elseif(DEFINED ENV{ARM64_LINUX})
        set(VCPKG_TARGET_TRIPLET "arm64-linux")
    elseif(DEFINED ENV{RISCV64_LINUX})
        set(VCPKG_TARGET_TRIPLET "riscv64-linux")
    else()
        set(VCPKG_TARGET_TRIPLET "x64-linux")
    endif()
endif()

project(xframes LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)
find_package(Stb REQUIRED)
find_package(ada CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(qjs CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)
# find_package(RPP REQUIRED)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)

# --- Janet 3-stage bootstrap ---
set(JANET_ROOT "${DEPS}/janet")
set(JANET_SRC_DIR "${JANET_ROOT}/src")
file(GLOB JANET_CORE_SOURCES "${JANET_SRC_DIR}/core/*.c")
file(GLOB JANET_BOOT_SOURCES "${JANET_SRC_DIR}/boot/*.c")
set(JANET_NO_FLAGS JANET_NO_EV JANET_NO_FFI JANET_NO_NET JANET_NO_DYNAMIC_MODULES)

# Stage 1: bootstrap interpreter
add_executable(janet_boot ${JANET_CORE_SOURCES} ${JANET_BOOT_SOURCES})
target_include_directories(janet_boot PRIVATE "${JANET_SRC_DIR}/include" "${JANET_SRC_DIR}/conf")
target_compile_definitions(janet_boot PRIVATE JANET_BOOTSTRAP ${JANET_NO_FLAGS})
if(WIN32)
    target_compile_definitions(janet_boot PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(janet_boot PRIVATE ws2_32)
endif()

# Stage 2: generate core image
set(JANET_IMAGE_FILE "${CMAKE_BINARY_DIR}/janet_core_image.c")
add_custom_command(
    OUTPUT "${JANET_IMAGE_FILE}"
    COMMAND ${CMAKE_COMMAND} -E env "$<TARGET_FILE:janet_boot>" "${JANET_ROOT}" image-only > "${JANET_IMAGE_FILE}"
    DEPENDS janet_boot "${JANET_ROOT}/src/boot/boot.janet"
    COMMENT "Generating Janet core image"
)

# Stage 3: static library (JANET_SINGLE_THREADED: janet_init sets thread-local VM;
# XFrames calls InitJanet on JS thread but Render on render thread)
add_library(janet_core STATIC ${JANET_CORE_SOURCES} "${JANET_IMAGE_FILE}")
target_include_directories(janet_core PUBLIC "${JANET_SRC_DIR}/include" "${JANET_SRC_DIR}/conf")
target_compile_definitions(janet_core PRIVATE ${JANET_NO_FLAGS} JANET_SINGLE_THREADED)
if(WIN32)
    target_compile_definitions(janet_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(janet_core PRIVATE ws2_32)
endif()

# Auto-copy core image for WASM builds
add_custom_command(TARGET janet_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${JANET_IMAGE_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/generated/janet_core_image.c"
    COMMENT "Copying Janet core image to app/generated/ for WASM"
)

file(GLOB LEPTONICA_SRC "${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src/*.c")

# Generate endianness.h for leptonica (x64 is little-endian)
file(WRITE "${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src/endianness.h" "#define L_LITTLE_ENDIAN\n")
add_compile_definitions(HAVE_LIBJPEG HAVE_LIBTIFF HAVE_LIBPNG NOMINMAX _USE_MATH_DEFINES)

set(OSM_STATIC_MAP_GENERATOR_SRC
        ${DEPS}/osm-static-map-generator/cpp/shared.cpp
        ${DEPS}/osm-static-map-generator/cpp/tiledownloader.cpp
        ${DEPS}/osm-static-map-generator/cpp/mapgenerator.cpp
        ${DEPS}/osm-static-map-generator/cpp/tilecache.cpp
)

# Copied from yoga's CMakeLists.txt
file(GLOB YOGA_SRC CONFIGURE_DEPENDS
        ${DEPS}/yoga/yoga/*.cpp
        ${DEPS}/yoga/yoga/**/*.cpp)

set(XFRAMES_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../app)

set(XFRAMES_SRC
        ${XFRAMES_BASE_PATH}/src/xframes.cpp
        ${XFRAMES_BASE_PATH}/src/imgui_renderer.cpp
        ${XFRAMES_BASE_PATH}/src/implot_renderer.cpp
        ${XFRAMES_BASE_PATH}/src/shared.cpp
        ${XFRAMES_BASE_PATH}/src/color_helpers.cpp
        ${XFRAMES_BASE_PATH}/src/imgui_helpers.cpp
        ${XFRAMES_BASE_PATH}/src/yoga_helpers.cpp
        ${XFRAMES_BASE_PATH}/src/disk_tile_cache.cpp
        ${XFRAMES_BASE_PATH}/src/element/layout_node.cpp
        ${XFRAMES_BASE_PATH}/src/element/element.cpp
        ${XFRAMES_BASE_PATH}/src/widget/widget.cpp
        ${XFRAMES_BASE_PATH}/src/widget/styled_widget.cpp
        ${XFRAMES_BASE_PATH}/src/widget/button.cpp
        ${XFRAMES_BASE_PATH}/src/widget/checkbox.cpp
        ${XFRAMES_BASE_PATH}/src/widget/color_picker.cpp
        ${XFRAMES_BASE_PATH}/src/widget/child.cpp
        ${XFRAMES_BASE_PATH}/src/widget/clipped_multi_line_text_renderer.cpp
        ${XFRAMES_BASE_PATH}/src/widget/collapsing_header.cpp
        ${XFRAMES_BASE_PATH}/src/widget/combo.cpp
        ${XFRAMES_BASE_PATH}/src/widget/group.cpp
        ${XFRAMES_BASE_PATH}/src/widget/image.cpp
        ${XFRAMES_BASE_PATH}/src/widget/input_text.cpp
        ${XFRAMES_BASE_PATH}/src/widget/item_tooltip.cpp
        ${XFRAMES_BASE_PATH}/src/widget/map_view.cpp
        ${XFRAMES_BASE_PATH}/src/widget/multi_slider.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_bar.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_heatmap.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_histogram.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_line.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_pie_chart.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_scatter.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_candlestick.cpp
        ${XFRAMES_BASE_PATH}/src/widget/separator.cpp
        ${XFRAMES_BASE_PATH}/src/widget/separator_text.cpp
        ${XFRAMES_BASE_PATH}/src/widget/progress_bar.cpp
        ${XFRAMES_BASE_PATH}/src/widget/color_indicator.cpp
        ${XFRAMES_BASE_PATH}/src/widget/slider.cpp
        ${XFRAMES_BASE_PATH}/src/widget/table.cpp
        ${XFRAMES_BASE_PATH}/src/widget/tabs.cpp
        ${XFRAMES_BASE_PATH}/src/widget/text.cpp
        ${XFRAMES_BASE_PATH}/src/widget/text_wrap.cpp
        ${XFRAMES_BASE_PATH}/src/widget/tree_node.cpp
        ${XFRAMES_BASE_PATH}/src/widget/window.cpp
        ${XFRAMES_BASE_PATH}/src/widget/js_canvas.cpp
        ${XFRAMES_BASE_PATH}/src/widget/lua_canvas.cpp
        ${XFRAMES_BASE_PATH}/src/widget/janet_canvas.cpp

)

add_library(xframes
        ${XFRAMES_SRC}
        ${OSM_STATIC_MAP_GENERATOR_SRC}
        ${LEPTONICA_SRC}
        ${YOGA_SRC}
        ${DEPS}/css-color-parser-cpp/csscolorparser.hpp
        ${DEPS}/css-color-parser-cpp/csscolorparser.cpp
)

target_include_directories(
        xframes PRIVATE
        ${XFRAMES_BASE_PATH}/include
        ${DEPS}/IconFontCppHeaders
        ${DEPS}/ReactivePlusPlus/src/rpp

        ${Stb_INCLUDE_DIR}
        ${DEPS}/yoga
        ${DEPS}/css-color-parser-cpp

        ${DEPS}/osm-static-map-generator/cpp
        ${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src
        ${LUA_INCLUDE_DIR}
        "${DEPS}/janet/src/include"
        "${DEPS}/janet/src/conf"
)

target_link_libraries(xframes PRIVATE
        imgui::imgui
        implot::implot
        nlohmann_json::nlohmann_json
        ada::ada
        JPEG::JPEG
        PNG::PNG
        TIFF::TIFF
        CURL::libcurl
        fmt::fmt
        glfw
        OpenGL::GL
        qjs
        ${LUA_LIBRARIES}
        sol2::sol2
        janet_core
)
