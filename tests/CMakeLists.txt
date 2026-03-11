set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

add_executable(test_evaluator test_evaluator.cpp)


# Link against the core and module libraries, as well as OpenCV for test assertions
target_link_libraries(test_evaluator PRIVATE
    VisionFlowCore
    VisionFlowModules
    ${OpenCV_LIBRARIES}
)

add_executable(test_loop test_loop.cpp)

target_link_libraries(test_loop PRIVATE
    VisionFlowCore
    VisionFlowModules
)
