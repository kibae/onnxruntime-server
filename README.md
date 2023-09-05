> [!WARNING]
> In development

# ONNX Runtime Server

- [ONNX: Open Neural Network Exchange](https://onnxruntime.ai/)
- Export machine learning models learned in various ML environments to ONNX files and provide inference services using ONNX Runtime Server.
- Provides inference and management functions via TCP or HTTP/HTTPS APIs.
- This project is part of the pg_onnx project. pg_onnx is an extension that allows you to perform inference using data
  within PostgreSQL.

----

- [Requirements](#requirements)
    - [1. ONNX Runtime](#1-onnx-runtime)
    - [2. Boost](#2-boost)
    - [3. CMake](#3-cmake)
    - [4. Cuda(optional, for GPU)](#4-cudaoptional-for-gpu)
    - [5. OpenSSL(optional, for HTTPS)](#5-openssloptional-for-https)

----

# Requirements

### 1. [ONNX Runtime](https://onnxruntime.ai/)

- Linux
    - Download binary from [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases)
    - Extract the downloaded file to `/usr/local/onnxruntime`
    - Add `/usr/local/onnxruntime/lib` to `LD_LIBRARY_PATH` or `/etc/ld.so.conf.d/onnxruntime.conf`
- Mac
  ```shell
  brew install onnxruntime
  ```

### 2. Boost

- Linux
  ```shell
  sudo apt install libboost-all-dev
  ```
- Mac
  ```shell
  brew install boost
  ```

### 3. [CMake](https://cmake.org/)

- Linux
  ```shell
  sudo apt install cmake
  ```
- Mac
  ```shell
  brew install cmake
  ```

### 4. CUDA(optional, for GPU)

- Linux
  ```shell
  sudo apt install nvidia-cuda-toolkit nvidia-cudnn
  ```

### 5. OpenSSL(optional, for HTTPS)

- Linux
  ```shell
  sudo apt install libssl-dev
  ```
- Mac
  ```shell
  brew install openssl
  ```

# TODO

- [x] CUDA Support
- [ ] Windows Support 
