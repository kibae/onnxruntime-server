cmake_minimum_required(VERSION 3.26)
project(onnx_runtime_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        task/task.cpp
        task/create_session.cpp
        task/execute_session.cpp
        task/destroy_session.cpp
        task/list_session.cpp
        task/get_session.cpp

        onnx/session_key.cpp
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
)

find_package(PkgConfig REQUIRED)
# onnxruntime
pkg_check_modules(onnxruntime REQUIRED libonnxruntime)

# boost
find_package(Boost COMPONENTS serialization system filesystem REQUIRED)

# openssl
find_package(OpenSSL)
if (OPENSSL_FOUND)
    add_definitions(-DHAS_OPENSSL)
    set(SOURCE_FILES ${SOURCE_FILES} transport/http/https_server.cpp transport/http/https_session.cpp)
endif ()

# cuda
find_package(CUDA)
if (CUDA_FOUND)
    message(STATUS "CUDA support found. Compiling with CUDA support.")
    enable_language(CUDA)
    add_definitions(-DHAS_CUDA)
    set(SOURCE_FILES ${SOURCE_FILES} onnx/execution/cuda_session.cpp)
else ()
    message(STATUS "CUDA support not found. Compiling without CUDA support.")
endif ()

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
include_directories(${Boost_INCLUDE_DIR} ${onnxruntime_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS} ${onnxruntime_LIBRARY_DIRS} ${OPENSSL_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${onnxruntime_LIBRARIES} ${OPENSSL_LIBRARIES})

add_subdirectory(standalone)

# test
find_package(GTest)
if (GTEST_FOUND)
    add_subdirectory(test)
endif ()
