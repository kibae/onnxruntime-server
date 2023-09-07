# ONNX Runtime Server

[![Github CI](https://github.com/kibae/onnxruntime-server/actions/workflows/cmake-linux.yml/badge.svg)](https://github.com/kibae/onnxruntime-server/actions/workflows/cmake-multi-platform.yml)
[![License](https://img.shields.io/github/license/kibae/onnxruntime-server)](https://github.com/kibae/onnxruntime-server/blob/main/LICENSE)

- [ONNX: Open Neural Network Exchange](https://onnxruntime.ai/)
- The ONNX Runtime Server is a server that provides TCP and HTTP/HTTPS REST APIs for ONNX inference.
- This project is part of the pg_onnx project. pg_onnx is an extension that allows you to perform inference using data
  within PostgreSQL.

----

<!-- TOC -->

- [Build ONNX Runtime Server](#build-onnx-runtime-server)
    - [Requirements](#requirements)
    - [Compile and Install](#compile-and-install)
- [Run the server](#run-the-server)
    - [Options](#options)
- [API](#api)

----

# Build ONNX Runtime Server

## Requirements

- [ONNX Runtime](https://onnxruntime.ai/)
    - <details>
      <summary>Linux</summary>

        - Use `download-onnxruntime-linux.sh` script
            - This script downloads the latest version of the binary and install to `/usr/local/onnxruntime`.
            - Also, add `/usr/local/onnxruntime/lib` to `/etc/ld.so.conf.d/onnxruntime.conf` and run `ldconfig`.
        - Or manually download binary from [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases).
      </details>
    - <details>
      <summary>Mac OS</summary>

        ```shell
        brew install onnxruntime
        ```      
      </details>

- [Boost](https://www.boost.org/)
    - <details>
      <summary>Ubuntu/Debian</summary>

        ```shell
        sudo apt install libboost-all-dev
        ```
      </details>
    - <details>
      <summary>Mac OS</summary>

        ```shell
        brew install boost
        ```      
      </details>

- [CMake](https://cmake.org/)
    - <details>
      <summary>Ubuntu/Debian</summary>

        ```shell
        sudo apt install cmake
        ```
      </details>
    - <details>
      <summary>Mac OS</summary>

        ```shell
        brew install cmake
        ```      
      </details>

- CUDA(*optional, for GPU*)
    - <details>
      <summary>Ubuntu/Debian</summary>

        ```shell
        sudo apt install nvidia-cuda-toolkit nvidia-cudnn
        ```
      </details>

- OpenSSL(*optional, for HTTPS*)
    - <details>
      <summary>Ubuntu/Debian</summary>

        ```shell
        sudo apt install libssl-dev
        ```
      </details>
    - <details>
      <summary>Mac OS</summary>

        ```shell
        brew install openssl
        ```      
      </details>

## Compile and Install

```shell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build --prefix /usr/local/onnxruntime-server
```

----

# Run the server

- You must enter the path option(`--model-dir`) where the models are located.
- You need to enable one of the following backends: TCP, HTTP, or HTTPS.

## Options

- Use the `-h`, `--help` option to see a full list of options.
- All options can be set as environment variables. This can be useful when operating in a container like Docker. But be
  careful. Command-line options are prioritized over environment variables.

  <details>
      <summary>All options</summary>

    | Option              | Environment                   | Description                                                                                                                                                             |
    |---------------------|-------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | `--workers`         | `ONNX_SERVER_WORKERS`         | Worker thread pool size.<br/>Default: `4`                                                                                                                               |
    | `--model-dir`       | `ONNX_SERVER_MODEL_DIR`       | Model directory path<br/>The onnx model files must be located in the following path:<br/>`${model_dir}/${model_name}/${model_version}/model.onnx`<br/>Default: `models` |
    | `--tcp-port`        | `ONNX_SERVER_TCP_PORT`        | Enable TCP backend and which port number to use.                                                                                                                        |
    | `--http-port`       | `ONNX_SERVER_HTTP_PORT`       | Enable HTTP backend and which port number to use.                                                                                                                       |
    | `--https-port`      | `ONNX_SERVER_HTTPS_PORT`      | Enable HTTPS backend and which port number to use.                                                                                                                      |
    | `--https-cert`      | `ONNX_SERVER_HTTPS_CERT`      | SSL Certification file path for HTTPS                                                                                                                                   |
    | `--https-key`       | `ONNX_SERVER_HTTPS_KEY`       | SSL Private key file path for HTTPS                                                                                                                                     |
    | `--log-level`       | `ONNX_SERVER_LOG_LEVEL`       | Log level(debug, info, warn, error, fatal)                                                                                                                              |
    | `--log-file`        | `ONNX_SERVER_LOG_FILE`        | Log file path.<br/>If not specified, logs will be printed to stdout.                                                                                                    |
    | `--access-log-file` | `ONNX_SERVER_ACCESS_LOG_FILE` | Access log file path.<br/>If not specified, logs will be printed to stdout.                                                                                             |

  </details>

----

# API
- [HTTP/HTTPS REST API](https://github.com/kibae/onnxruntime-server/wiki/REST-API(HTTP-HTTPS))
- [TCP API](https://github.com/kibae/onnxruntime-server/wiki/TCP-API)


