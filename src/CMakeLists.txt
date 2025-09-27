cmake_minimum_required(VERSION 3.22.0)
project(onnxruntime_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        task/task.cpp
        task/create_session.cpp
        task/execute_session.cpp
        task/destroy_session.cpp
        task/list_session.cpp
        task/get_session.cpp

        onnx/version.cpp
        onnx/providers.cpp
        onnx/session_key.cpp
        onnx/session_key_with_option.cpp
        onnx/session.cpp
        onnx/session_manager.cpp
        onnx/value_info.cpp
        onnx/execution/input_value.cpp
        onnx/execution/context.cpp

        transport/server.cpp
        transport/tcp/tcp_session.cpp
        transport/http/http_session_base.cpp
        transport/http/http_session.cpp
        transport/http/http_server.cpp
        transport/http/swagger_serve.cpp
        transport/http/swagger/swagger_index_html.cpp
        transport/http/swagger/swagger_openapi_yaml.cpp
)

find_package(PkgConfig REQUIRED)

# cuda, onnxruntime
find_package(CUDA)
if (NOT CUDA_FOUND)
    set(CUDA_SDK_ROOT_DIR "/usr/local/cuda")
    find_package(CUDA)
endif ()

if (CUDA_FOUND)
    message(STATUS "CUDA library found.")
    enable_language(CUDA)
    pkg_search_module(ONNX_RUNTIME IMPORTED_TARGET libonnxruntime_cuda)
    if (NOT ONNX_RUNTIME_FOUND)
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
        find_package(ONNXRuntime COMPONENTS cuda REQUIRED)
    endif ()

    if (ONNX_RUNTIME_WITH_CUDA_PROVIDER)
        message(STATUS "ONNX Runtime with CUDA provider found. Enabling CUDA support.")
        add_definitions(-DHAS_CUDA)
        set(SOURCE_FILES ${SOURCE_FILES} onnx/cuda/session_options.cpp)
    endif ()
else ()
    message(STATUS "CUDA library not found.")
    pkg_check_modules(ONNX_RUNTIME IMPORTED_TARGET libonnxruntime)
    if (NOT ONNX_RUNTIME_FOUND)
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
        find_package(ONNXRuntime REQUIRED)
    endif ()
endif ()

# boost
#set(Boost_USE_STATIC_LIBS ON)
if (WIN32)
    find_package(Boost COMPONENTS serialization system filesystem program_options thread asio beast REQUIRED)
else ()
    find_package(Boost COMPONENTS serialization filesystem program_options thread REQUIRED)
endif ()

# openssl
if (NOT NO_ONNXRUNTIME_SERVER_OPENSSL)
    #set(OPENSSL_USE_STATIC_LIBS ON)
    find_package(OpenSSL)
    if (OPENSSL_FOUND)
        add_definitions(-DHAS_OPENSSL)
        set(SOURCE_FILES ${SOURCE_FILES} transport/http/https_server.cpp transport/http/https_session.cpp)
    endif ()
endif ()

if (WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif ()

include_directories(${Boost_INCLUDE_DIR} ${ONNX_RUNTIME_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS} ${ONNX_RUNTIME_LIBRARY_DIRS} ${OPENSSL_LIBRARY_DIRS})

add_library(${PROJECT_NAME} STATIC SHARED ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${ONNX_RUNTIME_LIBRARIES} ${OPENSSL_LIBRARIES})

add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}>)
target_link_libraries(${PROJECT_NAME}_static ${Boost_LIBRARIES} ${ONNX_RUNTIME_LIBRARIES} ${OPENSSL_LIBRARIES})

add_subdirectory(standalone)

# test
if (NOT NO_ONNXRUNTIME_SERVER_TEST)
    find_package(GTest)
    if (GTEST_FOUND AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        enable_testing()
        add_subdirectory(test)
    endif ()
endif ()

# install
if (NOT NO_ONNXRUNTIME_SERVER_INSTALL AND NOT WIN32)
    set_target_properties(${PROJECT_NAME}_standalone PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

    install(TARGETS ${PROJECT_NAME}_standalone DESTINATION bin)
    install(TARGETS ${PROJECT_NAME} DESTINATION lib)
    install(TARGETS ${PROJECT_NAME}_static DESTINATION lib RENAME ${PROJECT_NAME})
    install(FILES onnxruntime_server.hpp DESTINATION include)
    install(FILES transport/http/http_server.hpp DESTINATION include/transport/http)
    install(FILES transport/tcp/tcp_server.hpp DESTINATION include/transport/tcp)
    install(DIRECTORY utils DESTINATION include/utils)
endif ()