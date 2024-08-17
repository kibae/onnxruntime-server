#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

curl -o sample/1/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/model1.onnx"
curl -o sample/2/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/model2.onnx"
