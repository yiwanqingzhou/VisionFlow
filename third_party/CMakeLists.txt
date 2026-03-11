cmake_minimum_required(VERSION 3.14)
project(vf_third_party LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Definitions
add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)

# Include directories
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui-node-editor
    ${CMAKE_CURRENT_SOURCE_DIR}/taskflow
)

# Sources
file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

file(GLOB NODE_EDITOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui-node-editor/*.cpp"
)

# Shared Library
add_library(vf_third_party SHARED
    ${IMGUI_SOURCES}
    ${NODE_EDITOR_SOURCES}
)

# External dependencies for the shared library
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(vf_third_party PRIVATE
    glfw
    OpenGL::GL
)

# Output directory for the shared library
set_target_properties(vf_third_party PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
)
