# VisionFlowCore Library
add_library(
  VisionFlowCore SHARED src/core/engine.cpp src/core/factory.cpp
                          src/utils/logger.cpp src/utils/global_config.cpp)

target_link_libraries(VisionFlowCore PUBLIC Threads::Threads ${OpenCV_LIBS})

target_include_directories(
  VisionFlowCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                          ${OpenCV_INCLUDE_DIRS})
