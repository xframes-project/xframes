cmake_minimum_required(VERSION 3.15...3.26)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

if(NOT DEFINED SKBUILD_PROJECT_NAME)
    message(WARNING "SKBUILD_PROJECT_NAME is not defined. Are you using scikit-build?")
endif()

project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(VCPKG_ROOT "${PROJECT_SOURCE_DIR}/../cpp/deps/vcpkg")
set(CMAKE_TOOLCHAIN_FILE "${PROJECT_SOURCE_DIR}/../cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake")

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

set(DEPS C:/dev/react-imgui/packages/dear-imgui/cpp/deps)
set(APP C:/dev/react-imgui/packages/dear-imgui/cpp/app)

set(CMAKE_CXX_STANDARD 20)
set(PYBIND11_NEWPYTHON ON)

find_package(pybind11 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(ada CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(fmt CONFIG REQUIRED)

file(GLOB YOGA_SRC CONFIGURE_DEPENDS
        ${DEPS}/yoga/yoga/*.cpp
        ${DEPS}/yoga/yoga/**/*.cpp)

pybind11_add_module(${SKBUILD_PROJECT_NAME}
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

        src/xframes.cpp
)

set_target_properties(${SKBUILD_PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${SKBUILD_PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${APP}/include

        ${Stb_INCLUDE_DIR}

        ${DEPS}/IconFontCppHeaders
        ${DEPS}/ReactivePlusPlus/src/rpp
        ${DEPS}/css-color-parser-cpp
        ${DEPS}/imgui
        ${DEPS}/imgui/backends
        ${DEPS}/implot
        ${DEPS}/json/include
        ${DEPS}/yoga
)

target_link_libraries(
        ${SKBUILD_PROJECT_NAME}
        PRIVATE
        ada::ada
        fmt::fmt
        glfw
        OpenGL::GL
)

install(TARGETS ${SKBUILD_PROJECT_NAME} LIBRARY DESTINATION .)