message(STATUS "Looking for ONNX Runtime")

set(ONNX_RUNTIME_ROOT_DIR ENV ONNX_ROOT /usr/local/onnxruntime /usr /usr/local /opt/homebrew C:/msys64/usr/local/onnxruntime)
set(ONNX_RUNTIME_INCLUDE_PATHS /usr/local/include/onnxruntime)
set(MACOS_ONNX_RUNTIME_INCLUDE_PATHS /opt/homebrew/include/onnxruntime /opt/homebrew/include/onnxruntime/core/session /usr/local/include/onnxruntime/core/session)

find_path(ONNX_RUNTIME_INCLUDE_DIRS onnxruntime_cxx_api.h PATHS ${ONNX_RUNTIME_ROOT_DIR} ${ONNX_RUNTIME_INCLUDE_PATHS} ${MACOS_ONNX_RUNTIME_INCLUDE_PATHS} PATH_SUFFIXES include)
find_library(ONNX_RUNTIME_LIBRARY onnxruntime PATHS ${ONNX_RUNTIME_ROOT_DIR} PATH_SUFFIXES lib)

find_library(ONNX_RUNTIME_CUDA_LIBRARY onnxruntime_providers_cuda PATHS ${ONNX_RUNTIME_ROOT_DIR} PATH_SUFFIXES lib)

if (ONNX_RUNTIME_LIBRARY)
    message(STATUS "Found ONNX Runtime include dir: ${ONNX_RUNTIME_INCLUDE_DIRS}")
    message(STATUS "Found ONNX Runtime library: ${ONNX_RUNTIME_LIBRARY}")
    get_filename_component(ONNX_RUNTIME_LIBRARY_DIRS ${ONNX_RUNTIME_LIBRARY} DIRECTORY)
    message(STATUS "Found ONNX Runtime library dir: ${ONNX_RUNTIME_LIBRARY_DIRS}")
    if (ONNX_RUNTIME_CUDA_LIBRARY)
        set(ONNX_RUNTIME_LIBRARIES ${ONNX_RUNTIME_LIBRARY} ${ONNX_RUNTIME_CUDA_LIBRARY})
    else ()
        set(ONNX_RUNTIME_LIBRARIES ${ONNX_RUNTIME_LIBRARY})
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
if ("cuda" IN_LIST ONNXRuntime_FIND_COMPONENTS)
    find_package_handle_standard_args(ONNXRuntime DEFAULT_MSG ONNX_RUNTIME_INCLUDE_DIRS ONNX_RUNTIME_LIBRARY_DIRS ONNX_RUNTIME_LIBRARIES ONNX_RUNTIME_LIBRARY ONNX_RUNTIME_CUDA_LIBRARY)
    mark_as_advanced(ONNX_RUNTIME_INCLUDE_DIRS ONNX_RUNTIME_LIBRARY_DIRS ONNX_RUNTIME_LIBRARIES ONNX_RUNTIME_LIBRARY ONNX_RUNTIME_CUDA_LIBRARY)
else ()
    find_package_handle_standard_args(ONNXRuntime DEFAULT_MSG ONNX_RUNTIME_INCLUDE_DIRS ONNX_RUNTIME_LIBRARY_DIRS ONNX_RUNTIME_LIBRARIES ONNX_RUNTIME_LIBRARY)
    mark_as_advanced(ONNX_RUNTIME_INCLUDE_DIRS ONNX_RUNTIME_LIBRARY_DIRS ONNX_RUNTIME_LIBRARIES ONNX_RUNTIME_LIBRARY)
endif ()

