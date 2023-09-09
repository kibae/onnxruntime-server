project(onnxruntime_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        task/task.cpp
        task/create_session.cpp
        task/execute_session.cpp
        task/destroy_session.cpp
        task/list_session.cpp
        task/get_session.cpp

        onnx/session_key.cpp
        onnx/session_key_with_option.cpp
        onnx/session.cpp
        onnx/session_manager.cpp
        onnx/value_info.cpp
        onnx/execution/input_value.cpp
        onnx/execution/context.cpp

        transport/server.cpp
        transport/tcp/tcp_server.cpp
        transport/tcp/tcp_session.cpp
        transport/http/http_session_base.cpp
        transport/http/http_server.cpp
        transport/http/http_session.cpp
        transport/http/swagger_serve.cpp
        transport/http/swagger/swagger_index_html.cpp
        transport/http/swagger/swagger_openapi_yaml.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# cuda, onnxruntime
find_package(CUDA)
if (NOT CUDA_FOUND)
    set(CUDA_SDK_ROOT_DIR "/usr/local/cuda")
    find_package(CUDA)
endif ()

if (CUDA_FOUND)
    message(STATUS "CUDA support found. Compiling with CUDA support.")
    enable_language(CUDA)
    add_definitions(-DHAS_CUDA)
    set(SOURCE_FILES ${SOURCE_FILES} onnx/cuda/session_options.cpp)
    find_package(ONNXRuntime COMPONENTS cuda REQUIRED)
else ()
    message(STATUS "CUDA support not found. Compiling without CUDA support.")
    find_package(ONNXRuntime REQUIRED)
endif ()

# boost
#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS serialization system filesystem program_options thread REQUIRED)

# openssl
#set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL)
if (OPENSSL_FOUND)
    add_definitions(-DHAS_OPENSSL)
    set(SOURCE_FILES ${SOURCE_FILES} transport/http/https_server.cpp transport/http/https_session.cpp)
endif ()


include_directories(${Boost_INCLUDE_DIR} ${ONNX_RUNTIME_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS} ${ONNX_RUNTIME_LIBRARY_DIR} ${OPENSSL_LIBRARY_DIRS})

add_library(${PROJECT_NAME} STATIC SHARED ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${ONNX_RUNTIME_LIBRARIES} ${OPENSSL_LIBRARIES})

add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}>)
target_link_libraries(${PROJECT_NAME}_static ${Boost_LIBRARIES} ${ONNX_RUNTIME_LIBRARIES} ${OPENSSL_LIBRARIES})

add_subdirectory(standalone)

# test
find_package(GTest)
if (GTEST_FOUND AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    enable_testing()
    add_subdirectory(test)
endif ()

# install
set_target_properties(onnxruntime_server_standalone PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

install(TARGETS onnxruntime_server_standalone DESTINATION bin)
install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(TARGETS ${PROJECT_NAME}_static DESTINATION lib RENAME ${PROJECT_NAME})
install(FILES onnxruntime_server.hpp DESTINATION include)
install(FILES transport/http/http_server.hpp DESTINATION include/transport/http)
install(FILES transport/tcp/tcp_server.hpp DESTINATION include/transport/tcp)
install(DIRECTORY utils DESTINATION include/utils)
