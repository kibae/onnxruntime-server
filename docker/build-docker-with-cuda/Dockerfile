FROM nvidia/cuda:12.2.0-runtime-ubuntu22.04

RUN apt update && apt install -y curl wget git vim build-essential cmake libboost-all-dev libssl-dev
RUN mkdir -p /app

WORKDIR /app
RUN git clone https://github.com/kibae/onnxruntime-server.git

WORKDIR /app/onnxruntime-server

