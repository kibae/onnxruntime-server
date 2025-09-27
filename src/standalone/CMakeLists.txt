project(onnxruntime_server_standalone)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME}
        main.cpp
        standalone.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime ${Boost_LIBRARIES} onnxruntime_server_static)
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif ()
