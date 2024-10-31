cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(VCPKG_TARGET_TRIPLET "x64-windows")

project(xframes LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)
find_package(Stb REQUIRED)
find_package(ada CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
# find_package(RPP REQUIRED)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)

file(GLOB LEPTONICA_SRC "${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src/*.c")

set(OSM_STATIC_MAP_GENERATOR_SRC
        ${DEPS}/osm-static-map-generator/cpp/shared.cpp
        ${DEPS}/osm-static-map-generator/cpp/tiledownloader.cpp
        ${DEPS}/osm-static-map-generator/cpp/mapgenerator.cpp
)

# Copied from yoga's CMakeLists.txt
file(GLOB YOGA_SRC CONFIGURE_DEPENDS
        ${DEPS}/yoga/yoga/*.cpp
        ${DEPS}/yoga/yoga/**/*.cpp)

set(XFRAMES_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../app)

set(XFRAMES_SRC
        ${XFRAMES_BASE_PATH}/src/reactimgui.cpp
        ${XFRAMES_BASE_PATH}/src/imgui_renderer.cpp
        ${XFRAMES_BASE_PATH}/src/implot_renderer.cpp
        ${XFRAMES_BASE_PATH}/src/shared.cpp
        ${XFRAMES_BASE_PATH}/src/color_helpers.cpp
        ${XFRAMES_BASE_PATH}/src/imgui_helpers.cpp
        ${XFRAMES_BASE_PATH}/src/yoga_helpers.cpp
        ${XFRAMES_BASE_PATH}/src/element/layout_node.cpp
        ${XFRAMES_BASE_PATH}/src/element/element.cpp
        ${XFRAMES_BASE_PATH}/src/widget/widget.cpp
        ${XFRAMES_BASE_PATH}/src/widget/styled_widget.cpp
        ${XFRAMES_BASE_PATH}/src/widget/button.cpp
        ${XFRAMES_BASE_PATH}/src/widget/checkbox.cpp
        ${XFRAMES_BASE_PATH}/src/widget/child.cpp
        ${XFRAMES_BASE_PATH}/src/widget/clipped_multi_line_text_renderer.cpp
        ${XFRAMES_BASE_PATH}/src/widget/collapsing_header.cpp
        ${XFRAMES_BASE_PATH}/src/widget/combo.cpp
        ${XFRAMES_BASE_PATH}/src/widget/group.cpp
        ${XFRAMES_BASE_PATH}/src/widget/image.cpp
        ${XFRAMES_BASE_PATH}/src/widget/input_text.cpp
        ${XFRAMES_BASE_PATH}/src/widget/item_tooltip.cpp
#        ${XFRAMES_BASE_PATH}/src/widget/map_view.cpp
        ${XFRAMES_BASE_PATH}/src/widget/multi_slider.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_line.cpp
        ${XFRAMES_BASE_PATH}/src/widget/plot_candlestick.cpp
        ${XFRAMES_BASE_PATH}/src/widget/separator.cpp
        ${XFRAMES_BASE_PATH}/src/widget/separator_text.cpp
        ${XFRAMES_BASE_PATH}/src/widget/slider.cpp
        ${XFRAMES_BASE_PATH}/src/widget/table.cpp
        ${XFRAMES_BASE_PATH}/src/widget/tabs.cpp
        ${XFRAMES_BASE_PATH}/src/widget/text.cpp
        ${XFRAMES_BASE_PATH}/src/widget/text_wrap.cpp
        ${XFRAMES_BASE_PATH}/src/widget/tree_node.cpp
        ${XFRAMES_BASE_PATH}/src/widget/window.cpp

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
)

target_link_libraries(xframes PRIVATE
        imgui::imgui
        implot::implot
        nlohmann_json::nlohmann_json
        ada::ada
        JPEG::JPEG
        PNG::PNG
        TIFF::TIFF
        ${CMAKE_CURRENT_SOURCE_DIR}/../glfw3dll.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../opengl32.lib
)
