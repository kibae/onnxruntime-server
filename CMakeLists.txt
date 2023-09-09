cmake_minimum_required(VERSION 3.22.0)
project(onnxruntime_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

enable_testing()
