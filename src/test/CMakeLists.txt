project(onnxruntime_server_test)
set(CMAKE_CXX_STANDARD 17)

enable_testing()

set(TEST_LIBS
        ${ONNX_RUNTIME_LIBRARIES}
        ${Boost_LIBRARIES}
        onnxruntime_server_static
        GTest::GTest
        GTest::Main
)

add_executable(lib_version test_lib_version.cpp)
target_link_libraries(lib_version PRIVATE ${TEST_LIBS})
add_test(NAME lib_version COMMAND lib_version)

if (WIN32)
    file(GLOB ONNXRUNTIME_LIB_FILES_TO_COPY "${ONNX_RUNTIME_LIBRARY_DIRS}/onnxruntime*")

    foreach (FILE ${ONNXRUNTIME_LIB_FILES_TO_COPY})
        add_custom_command(TARGET lib_version POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${FILE}
                $<TARGET_FILE_DIR:lib_version>
        )
    endforeach ()
endif ()

# __    __  .__   __.  __  .___________.
#|  |  |  | |  \ |  | |  | |           |
#|  |  |  | |   \|  | |  | `---|  |----`
#|  |  |  | |  . `  | |  |     |  |
#|  `--'  | |  |\   | |  |     |  |
# \______/  |__| \__| |__|     |__|
#

add_executable(unit_test_context unit/unit_test_context.cpp)
target_link_libraries(unit_test_context PRIVATE ${TEST_LIBS})
add_test(NAME unit_test_context COMMAND unit_test_context)

if (CUDA_FOUND)
    add_executable(unit_test_context_cuda unit/unit_test_context_cuda.cpp)
    target_link_libraries(unit_test_context_cuda PRIVATE ${TEST_LIBS})
    add_test(NAME unit_test_context_cuda COMMAND unit_test_context_cuda)
endif ()

add_executable(unit_test_session unit/unit_test_session.cpp)
target_link_libraries(unit_test_session PRIVATE ${TEST_LIBS})
add_test(NAME unit_test_session COMMAND unit_test_session)


# _______ ___    _______
#|   ____|__ \  |   ____|
#|  |__     ) | |  |__
#|   __|   / /  |   __|
#|  |____ / /_  |  |____
#|_______|____| |_______|

add_executable(e2e_test_tcp_server e2e/e2e_test_tcp_server.cpp)
target_link_libraries(e2e_test_tcp_server PRIVATE ${TEST_LIBS})
add_test(NAME e2e_test_tcp_server COMMAND e2e_test_tcp_server)

add_executable(e2e_test_http_server e2e/e2e_test_http_server.cpp)
target_link_libraries(e2e_test_http_server PRIVATE ${TEST_LIBS})
add_test(NAME e2e_test_http_server COMMAND e2e_test_http_server)

add_executable(e2e_test_http_swagger e2e/e2e_test_http_swagger.cpp)
target_link_libraries(e2e_test_http_swagger PRIVATE ${TEST_LIBS})
add_test(NAME e2e_test_http_swagger COMMAND e2e_test_http_swagger)

if (OPENSSL_FOUND)
    add_executable(e2e_test_https_server e2e/e2e_test_https_server.cpp)
    target_link_libraries(e2e_test_https_server PRIVATE ${TEST_LIBS})
    add_test(NAME e2e_test_https_server COMMAND e2e_test_https_server)
endif ()
