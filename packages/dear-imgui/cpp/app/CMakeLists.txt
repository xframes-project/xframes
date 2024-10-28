cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(VCPKG_TARGET_TRIPLET "x64-windows")

project(reactDearImgui LANGUAGES C CXX)

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

set(REACT_IMGUI_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../app)

set(REACT_IMGUI_SRC
        ${REACT_IMGUI_BASE_PATH}/src/shared.cpp
        ${REACT_IMGUI_BASE_PATH}/src/color_helpers.cpp
        ${REACT_IMGUI_BASE_PATH}/src/imgui_helpers.cpp
        ${REACT_IMGUI_BASE_PATH}/src/yoga_helpers.cpp
        ${REACT_IMGUI_BASE_PATH}/src/element/layout_node.cpp
        ${REACT_IMGUI_BASE_PATH}/src/element/element.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/widget.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/styled_widget.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/button.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/checkbox.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/child.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/clipped_multi_line_text_renderer.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/collapsing_header.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/combo.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/group.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/image.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/input_text.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/item_tooltip.cpp
#        ${REACT_IMGUI_BASE_PATH}/src/widget/map_view.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/multi_slider.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/plot_line.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/plot_candlestick.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/separator.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/separator_text.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/slider.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/table.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/tabs.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/text.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/text_wrap.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/tree_node.cpp
        ${REACT_IMGUI_BASE_PATH}/src/widget/window.cpp
        ${REACT_IMGUI_BASE_PATH}/src/reactimgui.cpp
        ${REACT_IMGUI_BASE_PATH}/src/imgui_renderer.cpp
        ${REACT_IMGUI_BASE_PATH}/src/implot_renderer.cpp
)

add_library(reactDearImgui
        ${REACT_IMGUI_SRC}
        ${OSM_STATIC_MAP_GENERATOR_SRC}
        ${LEPTONICA_SRC}
        ${YOGA_SRC}
        ${DEPS}/css-color-parser-cpp/csscolorparser.hpp
        ${DEPS}/css-color-parser-cpp/csscolorparser.cpp
)

target_include_directories(
        reactDearImgui PRIVATE
        ${REACT_IMGUI_BASE_PATH}/include
        ${DEPS}/IconFontCppHeaders
        ${DEPS}/ReactivePlusPlus/src/rpp

        ${Stb_INCLUDE_DIR}
        ${DEPS}/yoga
        ${DEPS}/css-color-parser-cpp

        ${DEPS}/osm-static-map-generator/cpp
        ${DEPS}/osm-static-map-generator/cpp/deps/leptonica/src
)

target_link_libraries(reactDearImgui PRIVATE imgui::imgui implot::implot nlohmann_json::nlohmann_json ada::ada JPEG::JPEG PNG::PNG TIFF::TIFF  ${CMAKE_CURRENT_SOURCE_DIR}/../glfw3dll.lib ${CMAKE_CURRENT_SOURCE_DIR}/../opengl32.lib)
