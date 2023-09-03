project(onnx_runtime_server_standalone)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} standalone.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime ${Boost_LIBRARIES} onnx_runtime_server)
