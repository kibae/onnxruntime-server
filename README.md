`In development`

# ONNX Runtime Server

- [ONNX: Open Neural Network Exchange](https://onnxruntime.ai/)
- Machine learning models trained in various environments can be exported as ONNX files to provide inference services.
- Provides inference and management functions via TCP or HTTP/HTTPS APIs.
- This project is part of the pg_onnx project. pg_onnx is an extension that allows you to perform inference using data
  within PostgreSQL.

# Requirements

## [ONNX Runtime](https://onnxruntime.ai/)

- Linux
    - Download binary from [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases)
    - Extract the downloaded file to `/usr/local/onnxruntime`
    - Add `/usr/local/onnxruntime/lib` to `LD_LIBRARY_PATH` or `/etc/ld.so.conf.d/onnxruntime.conf`
- Mac
  ```shell
  brew install onnxruntime
  ```

## Boost

- Linux
  ```shell
  sudo apt install libboost-all-dev
  ```
- Mac
  ```shell
  brew install boost
  ```

## Cuda(optional, for GPU)

- Linux
  ```shell
  sudo apt install nvidia-cuda-toolkit
  ```

## OpenSSL(optional, for HTTPS)

- Linux
  ```shell
  sudo apt install libssl-dev
  ```
- Mac
  ```shell
  brew install openssl
  ```

## [CMake](https://cmake.org/)

- Linux
  ```shell
  sudo apt install cmake
  ```
- Mac
  ```shell
    brew install cmake
    ```

# TODO

- [ ] Support for CUDA
- [ ] Support for Windows
