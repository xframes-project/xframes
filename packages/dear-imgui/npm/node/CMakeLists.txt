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
    elseif(DEFINED ENV{RISCV64_LINUX})
        set(VCPKG_TARGET_TRIPLET "riscv64-linux")
    else()
        set(VCPKG_TARGET_TRIPLET "x64-linux")
    endif()
endif()

project (xframes LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 23)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/deps)
set(APP ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/app)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(ada CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(fmt CONFIG REQUIRED)

include_directories(${CMAKE_JS_INC})

file(GLOB YOGA_SRC CONFIGURE_DEPENDS
        ${DEPS}/yoga/yoga/*.cpp
        ${DEPS}/yoga/yoga/**/*.cpp)

add_library(${PROJECT_NAME} SHARED
        ${DEPS}/css-color-parser-cpp/csscolorparser.hpp
        ${DEPS}/css-color-parser-cpp/csscolorparser.cpp

        ${DEPS}/imgui/imgui.cpp
        ${DEPS}/imgui/imgui_draw.cpp
        ${DEPS}/imgui/imgui_widgets.cpp
        ${DEPS}/imgui/imgui_tables.cpp
        ${DEPS}/imgui/backends/imgui_impl_opengl3.cpp
        ${DEPS}/imgui/backends/imgui_impl_glfw.cpp

        ${DEPS}/implot/implot.cpp
        ${DEPS}/implot/implot_items.cpp

        ${YOGA_SRC}

        ${APP}/src/shared.cpp
        ${APP}/src/color_helpers.cpp
        ${APP}/src/yoga_helpers.cpp
        ${APP}/src/imgui_helpers.cpp

        ${APP}/src/element/layout_node.cpp
        ${APP}/src/element/element.cpp

        ${APP}/src/widget/widget.cpp
        ${APP}/src/widget/styled_widget.cpp

        ${APP}/src/xframes.cpp
        ${APP}/src/imgui_renderer.cpp
        ${APP}/src/implot_renderer.cpp

        ${APP}/src/widget/button.cpp
        ${APP}/src/widget/checkbox.cpp
        ${APP}/src/widget/child.cpp
        ${APP}/src/widget/clipped_multi_line_text_renderer.cpp
        ${APP}/src/widget/collapsing_header.cpp
        ${APP}/src/widget/combo.cpp
        ${APP}/src/widget/group.cpp
        ${APP}/src/widget/image.cpp
        ${APP}/src/widget/input_text.cpp
        ${APP}/src/widget/item_tooltip.cpp
        ${APP}/src/widget/multi_slider.cpp
        ${APP}/src/widget/plot_candlestick.cpp
        ${APP}/src/widget/plot_line.cpp
        ${APP}/src/widget/separator.cpp
        ${APP}/src/widget/separator_text.cpp
        ${APP}/src/widget/slider.cpp
        ${APP}/src/widget/table.cpp
        ${APP}/src/widget/tabs.cpp
        ${APP}/src/widget/text.cpp
        ${APP}/src/widget/text_wrap.cpp
        ${APP}/src/widget/tree_node.cpp
        ${APP}/src/widget/window.cpp

        ./src/xframes-node.cpp

        ${SOURCE_FILES}
        ${CMAKE_JS_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME}
        ${CMAKE_JS_LIB}
        ada::ada
        fmt::fmt

        glfw
        OpenGL::GL
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
        ${APP}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api

        ${Stb_INCLUDE_DIR}

        ${DEPS}/node-addon-api
        ${DEPS}/node-api-headers/include
        ${DEPS}/IconFontCppHeaders
        ${DEPS}/ReactivePlusPlus/src/rpp
        ${DEPS}/css-color-parser-cpp
        ${DEPS}/imgui
        ${DEPS}/imgui/backends
        ${DEPS}/implot
        ${DEPS}/json/include
        ${DEPS}/yoga
        ${DEPS}/glfw/include
)

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=9)

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    # Generate node.lib
    execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

