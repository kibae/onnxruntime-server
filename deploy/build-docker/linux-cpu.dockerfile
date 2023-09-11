FROM ubuntu:latest AS builder

RUN apt update && apt install -y curl wget git build-essential cmake libboost-all-dev libssl-dev
RUN mkdir -p /app/source

WORKDIR /app/source
COPY src /app/source/onnxruntime-server
COPY cmake /app/source/onnxruntime-server/cmake
COPY deploy/build-docker/download-onnxruntime.sh /app/source/onnxruntime-server/

WORKDIR /app/source/onnxruntime-server

ARG TARGETPLATFORM
RUN case ${TARGETPLATFORM} in \
         "linux/amd64")  ./download-onnxruntime.sh linux x64  ;; \
         "linux/arm64")  ./download-onnxruntime.sh linux aarch64   ;; \
    esac

RUN cmake -DBoost_USE_STATIC_LIBS=ON -DOPENSSL_USE_STATIC_LIBS=ON -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel 4 --target onnxruntime_server_standalone
RUN cmake --install build --prefix /app/onnxruntime-server

# target
FROM ubuntu:latest AS target
COPY --from=builder /app/onnxruntime-server /app
COPY --from=builder /usr/local/onnxruntime /usr/local/onnxruntime

WORKDIR /app
RUN mkdir -p models logs certs

ENV ONNX_SERVER_CONFIG_PRIORITY=env
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda/lib64:/usr/local/onnxruntime/lib
ENTRYPOINT ["/app/bin/onnxruntime_server", "--model-dir", "models", "--log-file", "logs/app.log", "--access-log-file", "logs/access.log", "--tcp-port", "6432", "--http-port", "80"]
