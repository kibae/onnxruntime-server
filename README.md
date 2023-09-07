# ONNX Runtime Server

[![Github CI](https://github.com/kibae/onnxruntime-server/actions/workflows/cmake-linux.yml/badge.svg)](https://github.com/kibae/onnxruntime-server/actions/workflows/cmake-multi-platform.yml)
[![License](https://img.shields.io/github/license/kibae/onnxruntime-server)](https://github.com/kibae/onnxruntime-server/blob/main/LICENSE)

- [ONNX: Open Neural Network Exchange](https://onnxruntime.ai/)
- The ONNX Runtime Server is a server that provides TCP and HTTP/HTTPS REST APIs for ONNX inference.
- This project is part of the pg_onnx project. pg_onnx is an extension that allows you to perform inference using data
  within PostgreSQL.

----

- [Requirements](#requirements)
    - [1. ONNX Runtime](#1-onnx-runtime)
    - [2. Boost](#2-boost)
    - [3. CMake](#3-cmake)
    - [4. CUDA(optional, for GPU)](#4-cudaoptional-for-gpu)
    - [5. OpenSSL(optional, for HTTPS)](#5-openssloptional-for-https)
- [Wiki](#wiki)

----

# Requirements

### 1. [ONNX Runtime](https://onnxruntime.ai/)

- Linux
    - Use `download-onnxruntime-linux.sh` script
        - This script downloads the latest version of the binary and install to `/usr/local/onnxruntime`.
        - Also, add `/usr/local/onnxruntime/lib` to `/etc/ld.so.conf.d/onnxruntime.conf` and run `ldconfig`.
    - Or manually download binary from [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases).
- Mac
  ```shell
  brew install onnxruntime
  ```

### 2. [Boost](https://www.boost.org/)

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

# Wiki

- [TODO](https://github.com/kibae/onnxruntime-server/wiki/TODO)
