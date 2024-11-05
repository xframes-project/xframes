cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

if(DEFINED ENV{EMSDK})
    set(VCPKG_TARGET_TRIPLET "wasm32-emscripten")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE $ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake)

    execute_process(
        COMMAND "npm install"
        WORKING_DIRECTORY "$ENV{EMSDK}/upstream/emscripten"
    )
else()
    message(FATAL_ERROR "Environment variable EMSDK is not defined. Follow the README steps and source emsdk_env.* from the EMSDK root directory in this shell before running CMake")
endif()

project(xframes LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)
find_package(Stb REQUIRED)
find_package(ada CONFIG REQUIRED)
# find_package(RPP REQUIRED)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)
set(CMAKE_EXECUTABLE_SUFFIX ".mjs")

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
    ${XFRAMES_BASE_PATH}/src/widget/map_view.cpp
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
    ${XFRAMES_BASE_PATH}/src/xframes.cpp
    ${XFRAMES_BASE_PATH}/src/imgui_renderer.cpp
    ${XFRAMES_BASE_PATH}/src/implot_renderer.cpp
)

file(GLOB LIBTIFF_SRC CONFIGURE_DEPENDS
        ${DEPS}/osm-static-map-generator/cpp/deps/libtiff/libtiff/tif*.c)
list(REMOVE_ITEM LIBTIFF_SRC ${DEPS}/osm-static-map-generator/cpp/deps/libtiff/libtiff/tif_win32.c)

if (DEFINED EMSCRIPTEN)
	add_executable(xframes
        src/main.cpp
        ${XFRAMES_SRC}
        ${OSM_STATIC_MAP_GENERATOR_SRC}
        ${LEPTONICA_SRC}
        ${LIBTIFF_SRC}
        ${YOGA_SRC}
        ${DEPS}/css-color-parser-cpp/csscolorparser.hpp
        ${DEPS}/css-color-parser-cpp/csscolorparser.cpp
    )

    # https://github.com/emscripten-core/emscripten/issues/18476
    # -c -fexperimental-library

	set_target_properties(xframes PROPERTIES COMPILE_FLAGS "-O3 -Wall -Wformat -Wno-implicit-int -s DISABLE_EXCEPTION_CATCHING=0 --use-port=libjpeg --use-port=libpng")
    set_target_properties(xframes PROPERTIES LINK_FLAGS "--no-heap-copy --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../../assets@/assets -O3 -lembind --emit-tsd interface.d.ts --use-port=libjpeg --use-port=libpng -lhtml5.js -lhtml5_webgl.js -lglfw.js -s ENVIRONMENT='web' -s MODULARIZE=1 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_GLFW=3 -s USE_WEBGPU=1 -sFETCH -s NO_EXIT_RUNTIME=0 -s STANDALONE_WASM=0 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s STACK_OVERFLOW_CHECK=2 -s DISABLE_EXCEPTION_CATCHING=0 -s SINGLE_FILE=1")
    set_target_properties(xframes PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../npm/wasm/src/lib)

    # Reinstate line below once ReactivePlusPlus 2.1.2 gets released (preferably in the vcpkg repo)
    # target_link_libraries(xframes PRIVATE imgui::imgui implot::implot nlohmann_json::nlohmann_json RPP::rpp RPP::rppqt)

    # This currently breaks custom fonts - known issue
    # target_compile_definitions(xframes PUBLIC IMGUI_USE_WCHAR32)
endif()

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
    ${DEPS}/osm-static-map-generator/cpp/deps/libtiff/libtiff
    ${DEPS}/osm-static-map-generator/cpp/deps/libtiff/port
)

target_link_libraries(xframes PRIVATE imgui::imgui implot::implot nlohmann_json::nlohmann_json ada::ada)
