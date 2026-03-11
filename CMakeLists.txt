cmake_minimum_required(VERSION 3.14)
project(VisionFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force the executable to be built in the top-level build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# find dependencies
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)

# log configuration option (default off)
option(VF_LOG_SHOW_LOCATION "Show file and line number in logs" OFF)
if(VF_LOG_SHOW_LOCATION)
    add_definitions(-DVF_LOG_SHOW_LOCATION)
endif()

# Set third party path
set(THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")

include_directories(
    include
    ${THIRD_PARTY}
    ${THIRD_PARTY}/imgui
    ${THIRD_PARTY}/imgui/backends
    ${THIRD_PARTY}/imgui-node-editor
    ${THIRD_PARTY}/taskflow
)

# third_party configuration
option(VF_USE_PREBUILT_THIRD_PARTY "Use prebuilt third_party libraries" ON)

if(VF_USE_PREBUILT_THIRD_PARTY)
    set(VF_THIRD_PARTY_LIB_PATH "${CMAKE_SOURCE_DIR}/lib")
    add_library(vf_third_party SHARED IMPORTED)
    set_target_properties(vf_third_party PROPERTIES
        IMPORTED_LOCATION "${VF_THIRD_PARTY_LIB_PATH}/libvf_third_party.so"
        INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY};${THIRD_PARTY}/imgui;${THIRD_PARTY}/imgui/backends;${THIRD_PARTY}/imgui-node-editor;${THIRD_PARTY}/taskflow"
    )
else()
    add_subdirectory(third_party)
endif()

# include subdirectories
add_subdirectory(core)
add_subdirectory(modules)
add_subdirectory(ui)
add_subdirectory(tests)
