cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(VCPKG_TARGET_TRIPLET "x64-windows")

project (node-imgui LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 23)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)

find_package(OpenGL REQUIRED)
find_package(ada CONFIG REQUIRED)
find_package(Stb REQUIRED)

include_directories(${CMAKE_JS_INC})

file(GLOB YOGA_SRC CONFIGURE_DEPENDS
        ${DEPS}/yoga/yoga/*.cpp
        ${DEPS}/yoga/yoga/**/*.cpp)

add_library(${PROJECT_NAME} SHARED
        ../deps/css-color-parser-cpp/csscolorparser.hpp
        ../deps/css-color-parser-cpp/csscolorparser.cpp

        ../deps/imgui/imgui.cpp
        ../deps/imgui/imgui_draw.cpp
        ../deps/imgui/imgui_widgets.cpp
        ../deps/imgui/imgui_tables.cpp
        ../deps/imgui/backends/imgui_impl_opengl3.cpp
        ../deps/imgui/backends/imgui_impl_glfw.cpp

        ../deps/implot/implot.cpp
        ../deps/implot/implot_items.cpp

        ${YOGA_SRC}

        ../app/src/shared.cpp
        ../app/src/color_helpers.cpp
        ../app/src/yoga_helpers.cpp
        ../app/src/imgui_helpers.cpp

        ../app/src/element/layout_node.cpp
        ../app/src/element/element.cpp

        ../app/src/widget/widget.cpp
        ../app/src/widget/styled_widget.cpp

        ../app/src/reactimgui.cpp
        ../app/src/imgui_renderer.cpp
        ../app/src/implot_renderer.cpp

        ../app/src/widget/button.cpp
        ../app/src/widget/checkbox.cpp
        ../app/src/widget/child.cpp
        ../app/src/widget/clipped_multi_line_text_renderer.cpp
        ../app/src/widget/collapsing_header.cpp
        ../app/src/widget/combo.cpp
        ../app/src/widget/group.cpp
        ../app/src/widget/image.cpp
        ../app/src/widget/input_text.cpp
        ../app/src/widget/item_tooltip.cpp
        ../app/src/widget/multi_slider.cpp
        ../app/src/widget/plot_candlestick.cpp
        ../app/src/widget/plot_line.cpp
        ../app/src/widget/separator.cpp
        ../app/src/widget/separator_text.cpp
        ../app/src/widget/slider.cpp
        ../app/src/widget/table.cpp
        ../app/src/widget/tabs.cpp
        ../app/src/widget/text.cpp
        ../app/src/widget/text_wrap.cpp
        ../app/src/widget/tree_node.cpp
        ../app/src/widget/window.cpp

        hello.cpp

        ${SOURCE_FILES}
        ${CMAKE_JS_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME}
        ${CMAKE_JS_LIB}
        ada::ada
        ${CMAKE_CURRENT_SOURCE_DIR}/glfw3dll.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/opengl32.lib
)

# Include Node-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE
        ${NODE_ADDON_API_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../app/include
        ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-

        ${Stb_INCLUDE_DIR}

        ../deps/node-addon-api
        ../deps/node-api-headers/include
        ../deps/IconFontCppHeaders
        ../deps/ReactivePlusPlus/src/rpp
        ../deps/css-color-parser-cpp
        ../deps/imgui
        ../deps/imgui/backends
        ../deps/implot
        ../deps/json/include
        ../deps/yoga
        ../deps/glfw/include
)

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=9)

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    # Generate node.lib
    execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

#if (MSVC)

#else()

#endif()