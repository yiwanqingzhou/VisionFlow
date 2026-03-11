# Collect third-party ImGui sources
file(GLOB IMGUI_SOURCES 
    "${THIRD_PARTY}/imgui/*.cpp"
    "${THIRD_PARTY}/imgui/backends/imgui_impl_glfw.cpp"
    "${THIRD_PARTY}/imgui/backends/imgui_impl_opengl3.cpp"
)

file(GLOB NODE_EDITOR_SOURCES
    "${THIRD_PARTY}/imgui-node-editor/*.cpp"
)

# Build Executable
add_executable(${PROJECT_NAME} 
    src/ui/main.cpp 
    src/ui/ui_manager.cpp
    src/ui/node_manager.cpp
    src/ui/texture_manager.cpp
    ${IMGUI_SOURCES}
    ${NODE_EDITOR_SOURCES}
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE VisionFlowCore
    PRIVATE -Wl,--no-as-needed VisionFlowModules -Wl,--as-needed
    PRIVATE glfw 
    PRIVATE OpenGL::GL 
    PRIVATE Threads::Threads 
    PRIVATE dl
)

target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
)
