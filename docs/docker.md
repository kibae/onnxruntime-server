# ONNX Runtime Server

- **The ONNX Runtime Server is a server that provides TCP and HTTP/HTTPS REST APIs for ONNX inference.**
- https://github.com/kibae/onnxruntime-server

# Supported tags and respective Dockerfile links

- [`1.20.2-linux-cuda12`](https://github.com/kibae/onnxruntime-server/blob/main/deploy/build-docker/linux-cuda12.dockerfile) amd64(CUDA 12.x, cuDNN 9.x)
- [`1.20.2-linux-cpu`](https://github.com/kibae/onnxruntime-server/blob/main/deploy/build-docker/linux-cpu.dockerfile) amd64, arm64

# How to use this image

- onnxruntime-server supports hardware acceleration with CUDA. To use the `--gpus all` option when running `docker run`,
  the `nvidia-container-toolkit` package must be installed on the host OS.
  ```shell
  sudo apt install nvidia-container-toolkit
  ```
- This example assumes that your onnx files are located in the `/your_model_dir` directory on the host OS.
  See [this document](https://github.com/kibae/onnxruntime-server#run-the-server) for paths and
  naming conventions for onnx files
    - `${model_dir}/${model_name}/${model_version}/model.onnx`
- Available environment variables can be found at
    - https://github.com/kibae/onnxruntime-server#run-the-server

## General usage

- After the docker container is up, you can use the REST API (http://localhost or https://localhost).
    - API documentation will be available at http://localhost/api-docs.

```shell
DOCKER_IMAGE=kibae/onnxruntime-server:1.20.2-linux-cuda12 # or kibae/onnxruntime-server:1.20.2-linux-cpu	

docker pull ${DOCKER_IMAGE}

# simple http backend
docker run --name onnxruntime_server_container -d --rm --gpus all \
  -p 80:80 \
  -v "/your_model_dir:/app/models" \
  -v "/your_log_dir:/app/logs" \
  -e "ONNX_SERVER_SWAGGER_URL_PATH=/api-docs" \
  ${DOCKER_IMAGE}

# with https backend. cert, key files must be located in the /your_cert_dir directory on the host OS.
docker run --name onnxruntime_server_container -d --rm --gpus all \
  -p 80:80 \
  -p 443:443 \
  -v "/your_model_dir:/app/models" \
  -v "/your_log_dir:/app/logs" \
  -v "/your_cert_dir:/app/certs" \
  -e "ONNX_SERVER_SWAGGER_URL_PATH=/api-docs" \
  -e "ONNX_SERVER_HTTPS_PORT=443" \
  -e "ONNX_SERVER_HTTPS_CERT=/app/certs/cert.pem" \
  -e "ONNX_SERVER_HTTPS_KEY=/app/certs/key.pem" \
  ${DOCKER_IMAGE}
```

## Using docker-compose

- [docker-compose.yml](https://github.com/kibae/onnxruntime-server/blob/main/deploy/build-docker/docker-compose.yaml)
  example is available in the repository.

### Simple one for HTTP backend

```yaml
services:
  # Available environment variables can be found at
  # https://github.com/kibae/onnxruntime-server#run-the-server

  onnxruntime_server_simple:
    # After the docker container is up, you can use the REST API (http://localhost:8080).
    # API documentation will be available at http://localhost:8080/api-docs.
    image: kibaes/onnxruntime-server:1.20.2-linux-cuda12
    ports:
      - "8080:80" # for http backend
    volumes:
      # for model files
      # https://github.com/kibae/onnxruntime-server#run-the-server
      - /your_model_dir:/app/models

      # for log files
      - /your_log_dir:/app/logs
    environment:
      # for swagger(optional)
      - ONNX_SERVER_SWAGGER_URL_PATH=/api-docs
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 1
              capabilities: [ gpu ]
```

### Advanced usage for HTTPS backend

```yaml
services:
  # Available environment variables can be found at
  # https://github.com/kibae/onnxruntime-server#run-the-server

  onnxruntime_server_advanced:
    # After the docker container is up, you can use the REST API (http://localhost, https://localhost).
    # API documentation wl be available at http://localhost/api-docs.
    image: kibaes/onnxruntime-server:1.20.2-linux-cuda12
    ports:
      - "80:80" # for http backend
      - "443:443" # for https backend
      - "8001:8001" # for tcp backend. binary protocol
    volumes:
      # for model files
      # https://github.com/kibae/onnxruntime-server#run-the-server
      - /your_model_dir:/app/models

      # for log files
      - /your_log_dir:/app/logs

      # for cert files
      - /your_cert_dir:/app/certs
    environment:
      # for https backend
      - ONNX_SERVER_HTTPS_PORT=443

      # https backend needs cert, key files
      - ONNX_SERVER_HTTPS_CERT=/app/certs/cert.pem
      - ONNX_SERVER_HTTPS_KEY=/app/certs/key.pem

      # for onnx session preparation
      - ONNX_SERVER_PREPARE_MODEL="model1:v1(cuda=true) model1:v2(cuda=0) model2:v2"

      # for swagger(optional)
      - ONNX_SERVER_SWAGGER_URL_PATH=/api-docs
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 1
              capabilities: [ gpu ]
```