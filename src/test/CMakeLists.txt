project(onnxruntime_server_test)
set(CMAKE_CXX_STANDARD 17)

enable_testing()

# test_onnxruntime_server_context
add_executable(test_onnxruntime_server_context context_test.cpp)
target_link_libraries(test_onnxruntime_server_context PRIVATE onnxruntime ${Boost_LIBRARIES} onnxruntime_server GTest::GTest GTest::Main)
add_test(NAME test_onnxruntime_server_context COMMAND test_onnxruntime_server_context)

if (CUDA_FOUND)
    # test_onnxruntime_server_context_cuda
    add_executable(test_onnxruntime_server_context_cuda context_cuda_test.cpp)
    target_link_libraries(test_onnxruntime_server_context_cuda PRIVATE onnxruntime ${Boost_LIBRARIES} onnxruntime_server GTest::GTest GTest::Main)
    add_test(NAME test_onnxruntime_server_context_cuda COMMAND test_onnxruntime_server_context_cuda)
endif ()

# test_onnxruntime_server_tcp
add_executable(test_onnxruntime_server_tcp tcp_server_test.cpp)
target_link_libraries(test_onnxruntime_server_tcp PRIVATE onnxruntime ${Boost_LIBRARIES} onnxruntime_server GTest::GTest GTest::Main)
add_test(NAME test_onnxruntime_server_tcp COMMAND test_onnxruntime_server_tcp)

# test_onnxruntime_server_http
add_executable(test_onnxruntime_server_http http_server_test.cpp)
target_link_libraries(test_onnxruntime_server_http PRIVATE onnxruntime ${Boost_LIBRARIES} onnxruntime_server GTest::GTest GTest::Main)
add_test(NAME test_onnxruntime_server_http COMMAND test_onnxruntime_server_http)

if (OPENSSL_FOUND)
    # test_onnxruntime_server_https
    add_executable(test_onnxruntime_server_https https_server_test.cpp)
    target_link_libraries(test_onnxruntime_server_https PRIVATE onnxruntime ${Boost_LIBRARIES} onnxruntime_server GTest::GTest GTest::Main)
    add_test(NAME test_onnxruntime_server_https COMMAND test_onnxruntime_server_https)
endif ()
