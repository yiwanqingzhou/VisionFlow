cmake_minimum_required(VERSION 3.14)
project(TaskNodeFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动寻找依赖
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)

# 设置库路径 (对应 Dockerfile 中的 /third_party)
set(THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")

include_directories(
    include
    ${THIRD_PARTY}
    ${THIRD_PARTY}/imgui
    ${THIRD_PARTY}/imgui/backends
    ${THIRD_PARTY}/imgui-node-editor
    ${THIRD_PARTY}/taskflow
)

# 收集源文件
file(GLOB IMGUI_SOURCES 
    "${THIRD_PARTY}/imgui/*.cpp"
    "${THIRD_PARTY}/imgui/backends/imgui_impl_glfw.cpp"
    "${THIRD_PARTY}/imgui/backends/imgui_impl_opengl3.cpp"
)

file(GLOB NODE_EDITOR_SOURCES
    "${THIRD_PARTY}/imgui-node-editor/*.cpp"
)

add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/engine.cpp
    src/factory.cpp
    # src/ui_nodes.cpp
    src/ui_manager.cpp
    src/node_manager.cpp
    ${IMGUI_SOURCES}
    ${NODE_EDITOR_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL Threads::Threads dl)
