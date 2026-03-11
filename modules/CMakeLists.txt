file(GLOB MODULE_SOURCES CONFIGURE_DEPENDS "src/modules/*.cpp")

# VisionFlowModules Library
add_library(VisionFlowModules SHARED ${MODULE_SOURCES})

# Depends on Core, OpenCV, and OpenGL (if any textures are needed for some reason, though we intend to decouple this)
target_link_libraries(VisionFlowModules PUBLIC VisionFlowCore ${OpenCV_LIBS})

target_include_directories(VisionFlowModules PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${OpenCV_INCLUDE_DIRS}
)
