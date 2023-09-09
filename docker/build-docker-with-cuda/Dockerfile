FROM nvidia/cuda:12.2.0-devel-ubuntu22.04

RUN apt update && apt install -y curl wget git vim build-essential cmake libboost-all-dev libssl-dev
RUN mkdir -p /app

WORKDIR /app
RUN git clone https://github.com/kibae/onnxruntime-server.git

WORKDIR /app/onnxruntime-server
RUN docker/build-docker-with-cuda/download-onnxruntime-linux.sh
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel 4
