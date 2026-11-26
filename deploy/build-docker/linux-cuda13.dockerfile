FROM nvidia/cuda:13.0.3-cudnn-devel-ubuntu24.04 AS build-base

RUN apt update && apt install -y curl wget python3 git build-essential cmake pkg-config libboost-all-dev libssl-dev
RUN mkdir -p /app/source

WORKDIR /app/source
FROM build-base AS ort-sdk
WORKDIR /app/source/onnxruntime-server
COPY deploy/build-docker/download-onnxruntime.sh deploy/build-docker/release-assets.sh deploy/build-docker/VERSION ./
RUN ./download-onnxruntime.sh linux x64-gpu_cuda13

FROM build-base AS builder
COPY --from=ort-sdk /usr/local/onnxruntime /usr/local/onnxruntime
WORKDIR /app/source/onnxruntime-server
COPY src .
COPY cmake ./cmake
COPY deploy/build-docker/VERSION ./
RUN . ./VERSION && test "$(cat /usr/local/onnxruntime/VERSION_NUMBER)" = "$ORT_VERSION"
RUN cmake -DCUDA_SDK_ROOT_DIR=/usr/local/cuda-13 -DBoost_USE_STATIC_LIBS=ON -DOPENSSL_USE_STATIC_LIBS=ON -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel 8 --target onnxruntime_server_standalone
RUN cmake --install build --prefix /app/onnxruntime-server

# target
FROM nvidia/cuda:13.0.3-cudnn-runtime-ubuntu24.04 AS target
COPY --from=builder /app/onnxruntime-server /app
COPY --from=builder /usr/local/onnxruntime /usr/local/onnxruntime

WORKDIR /app
RUN mkdir -p models logs certs

ENV ONNX_SERVER_CONFIG_PRIORITY=env
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda-13/lib64:/usr/local/onnxruntime/lib
ENTRYPOINT ["/app/bin/onnxruntime_server", "--model-dir", "models", "--log-file", "logs/app.log", "--access-log-file", "logs/access.log", "--tcp-port", "6432", "--http-port", "80"]
