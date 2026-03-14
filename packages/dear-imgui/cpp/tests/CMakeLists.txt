# 'Google_test' is the subproject name
cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)
set(VCPKG_TARGET_ARCHITECTURE "x64")

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

project(Google_tests)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Stb REQUIRED)
find_package(ada CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(CURL REQUIRED)
find_package(qjs CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 23)

if(MSVC)
    # Use /Z7 to embed debug info in .obj files instead of a shared .pdb,
    # avoiding PDB write contention with many source files
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

add_compile_definitions(HAVE_LIBJPEG HAVE_LIBTIFF HAVE_LIBPNG NOMINMAX _USE_MATH_DEFINES)

enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# 'lib' is the folder with Google Test sources
add_subdirectory(../deps/googletest tests-binary)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

file(GLOB YOGA_SRC CONFIGURE_DEPENDS
        ${DEPS}/yoga/yoga/*.cpp
        ${DEPS}/yoga/yoga/**/*.cpp)

file(GLOB LEPTONICA_SRC "${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src/*.c")
file(WRITE "${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src/endianness.h" "#define L_LITTLE_ENDIAN\n")

set(OSM_STATIC_MAP_GENERATOR_SRC
        ${DEPS}/osm-static-map-generator/cpp/shared.cpp
        ${DEPS}/osm-static-map-generator/cpp/tiledownloader.cpp
        ${DEPS}/osm-static-map-generator/cpp/mapgenerator.cpp
        ${DEPS}/osm-static-map-generator/cpp/tilecache.cpp
)

# imgui + implot compiled from submodule (matches Node addon build, v1.92.6)
set(IMGUI_SRC
        ${DEPS}/imgui/imgui.cpp
        ${DEPS}/imgui/imgui_draw.cpp
        ${DEPS}/imgui/imgui_widgets.cpp
        ${DEPS}/imgui/imgui_tables.cpp
        ${DEPS}/imgui/misc/cpp/imgui_stdlib.cpp
        ${DEPS}/imgui/backends/imgui_impl_opengl3.cpp
        ${DEPS}/imgui/backends/imgui_impl_glfw.cpp
)

set(IMPLOT_SRC
        ${DEPS}/implot/implot.cpp
        ${DEPS}/implot/implot_items.cpp
)

# 'Google_Tests_run' is the target name
add_executable(Google_Tests_run
        ${DEPS}/css-color-parser-cpp/csscolorparser.hpp
        ${DEPS}/css-color-parser-cpp/csscolorparser.cpp

        ${YOGA_SRC}
        ${OSM_STATIC_MAP_GENERATOR_SRC}
        ${LEPTONICA_SRC}
        ${IMGUI_SRC}
        ${IMPLOT_SRC}

        ../app/src/shared.cpp
        ../app/src/color_helpers.cpp
        ../app/src/yoga_helpers.cpp
        ../app/src/imgui_helpers.cpp

        ../app/src/element/layout_node.cpp
        ../app/src/element/element.cpp

        ../app/src/widget/widget.cpp
        ../app/src/widget/styled_widget.cpp

        ../app/src/widget/button.cpp
        ../app/src/widget/checkbox.cpp
        ../app/src/widget/color_picker.cpp
        ../app/src/widget/child.cpp
        ../app/src/widget/clipped_multi_line_text_renderer.cpp
        ../app/src/widget/collapsing_header.cpp
        ../app/src/widget/combo.cpp
        ../app/src/widget/group.cpp
        ../app/src/widget/image.cpp
        ../app/src/widget/input_text.cpp
        ../app/src/widget/item_tooltip.cpp
        ../app/src/widget/map_view.cpp
        ../app/src/widget/multi_slider.cpp
        ../app/src/widget/plot_bar.cpp
        ../app/src/widget/plot_candlestick.cpp
        ../app/src/widget/plot_histogram.cpp
        ../app/src/widget/plot_heatmap.cpp
        ../app/src/widget/plot_pie_chart.cpp
        ../app/src/widget/plot_line.cpp
        ../app/src/widget/plot_scatter.cpp
        ../app/src/widget/separator.cpp
        ../app/src/widget/separator_text.cpp
        ../app/src/widget/progress_bar.cpp
        ../app/src/widget/color_indicator.cpp
        ../app/src/widget/slider.cpp
        ../app/src/widget/table.cpp
        ../app/src/widget/tabs.cpp
        ../app/src/widget/text.cpp
        ../app/src/widget/text_wrap.cpp
        ../app/src/widget/tree_node.cpp
        ../app/src/widget/window.cpp

        ../app/src/xframes.cpp
        ../app/src/disk_tile_cache.cpp
        ../app/src/imgui_renderer.cpp
        ../app/src/implot_renderer.cpp

        shared_functions_test.cpp
        color_helpers_test.cpp
        yoga_helpers_test.cpp
        imgui_helpers_test.cpp
        element/layout_node_test.cpp
        element/element_test.cpp
        xframes_test.cpp
        quickjs_test.cpp
)

target_include_directories(
        Google_Tests_run PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../app/include
        ${DEPS}/IconFontCppHeaders
        ${DEPS}/ReactivePlusPlus/src/rpp

        ${Stb_INCLUDE_DIR}
        ${DEPS}/yoga
        ${DEPS}/css-color-parser-cpp

        ${DEPS}/imgui
        ${DEPS}/imgui/backends
        ${DEPS}/imgui/misc/cpp
        ${DEPS}/implot

        ${DEPS}/osm-static-map-generator/cpp
        ${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src
        ${DEPS}/osm-static-map-generator/cpp/deps/libtiff/libtiff
        ${DEPS}/osm-static-map-generator/cpp/deps/libtiff/port
)

target_link_libraries(Google_Tests_run
        gtest
        gmock
        gmock_main
        gtest_main
        nlohmann_json::nlohmann_json
        ada::ada
        glfw
        OpenGL::GL
        fmt::fmt
        JPEG::JPEG
        PNG::PNG
        TIFF::TIFF
        CURL::libcurl
        qjs
)
