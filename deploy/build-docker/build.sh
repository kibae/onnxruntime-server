#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

VERSION=1.0.0

# build docker image amd64, arm64, armv7l, armv6l
#docker buildx build --platform linux/amd64,linux/arm64 -t onnxruntime:${VERSION} -f Dockerfile.cpu .

#exit 0

# build docker image amd64-cuda
docker buildx build --platform linux/amd64 -t onnxruntime:build-${VERSION}-cuda -f Dockerfile.build-amd64-cuda --load .
docker rm temp_container || true
docker create --name temp_container onnxruntime:build-${VERSION}-cuda
docker cp temp_container:/app/onnxruntime-server ./out/amd64-cuda
docker rm temp_container || true

