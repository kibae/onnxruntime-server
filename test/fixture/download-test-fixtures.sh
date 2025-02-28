#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

if [ ! -f sample/1/model.onnx ]; then
  curl -o sample/1/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/model1.onnx"
fi

if [ ! -f sample/2/model.onnx ]; then
  curl -o sample/2/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/model2.onnx"
fi

if [ ! -f sample/3.onnx ]; then
  cp sample/1/model.onnx sample/3.onnx
fi
