cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(VCPKG_TARGET_TRIPLET "x64-windows")

project (node-imgui LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 23)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)

find_package(OpenGL REQUIRED)
find_package(Stb REQUIRED)

include_directories(${CMAKE_JS_INC})

add_library(${PROJECT_NAME} SHARED
        ../deps/imgui/imgui.cpp
        ../deps/imgui/imgui_draw.cpp
        ../deps/imgui/imgui_tables.cpp
        ../deps/imgui/imgui_widgets.cpp

        ../deps/imgui/backends/imgui_impl_glfw.cpp
        ../deps/imgui/backends/imgui_impl_opengl3.cpp

        ../deps/implot/implot.cpp
        ../deps/implot/implot_items.cpp

        hello.cc

        ${SOURCE_FILES}
        ${CMAKE_JS_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME}
        ${NODE_ADDON_API_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api

        ${CMAKE_JS_LIB}

        ${Stb_INCLUDE_DIR}

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

        ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api

        ${Stb_INCLUDE_DIR}

        ../deps/glfw/include
        ../deps/imgui
        ../deps/imgui/backends
        ../deps/implot
)

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=9)

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    # Generate node.lib
    execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()
