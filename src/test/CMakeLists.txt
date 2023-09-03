project(onnx_runtime_server_test)
set(CMAKE_CXX_STANDARD 17)

enable_testing()

# test_onnx_runtime_server_context
add_executable(test_onnx_runtime_server_context context_test.cpp)
target_link_libraries(test_onnx_runtime_server_context PRIVATE onnxruntime ${Boost_LIBRARIES} onnx_runtime_server GTest::GTest GTest::Main)
add_test(NAME test_onnx_runtime_server_context COMMAND test_onnx_runtime_server_context)

# test_onnx_runtime_server_tcp
add_executable(test_onnx_runtime_server_tcp tcp_server_test.cpp)
target_link_libraries(test_onnx_runtime_server_tcp PRIVATE onnxruntime ${Boost_LIBRARIES} onnx_runtime_server GTest::GTest GTest::Main)
add_test(NAME test_onnx_runtime_server_tcp COMMAND test_onnx_runtime_server_tcp)

# test_onnx_runtime_server_http
add_executable(test_onnx_runtime_server_http http_server_test.cpp)
target_link_libraries(test_onnx_runtime_server_http PRIVATE onnxruntime ${Boost_LIBRARIES} onnx_runtime_server GTest::GTest GTest::Main)
add_test(NAME test_onnx_runtime_server_http COMMAND test_onnx_runtime_server_http)

if (OPENSSL_FOUND)
    # test_onnx_runtime_server_https
    add_executable(test_onnx_runtime_server_https https_server_test.cpp)
    target_link_libraries(test_onnx_runtime_server_https PRIVATE onnxruntime ${Boost_LIBRARIES} onnx_runtime_server GTest::GTest GTest::Main)
    add_test(NAME test_onnx_runtime_server_https COMMAND test_onnx_runtime_server_https)
endif ()
