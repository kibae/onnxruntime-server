#!/usr/bin/env bash
set -e

cd "$(dirname "$0")" || exit

curl -fL -o sample/1/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/model1.onnx"
curl -fL -o sample/2/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/model2.onnx"
cp sample/1/model.onnx sample/3.onnx

mkdir -p sample/datatypes
curl -fL -o sample/datatypes/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/datatypes.onnx"

mkdir -p sample/scalar
curl -fL -o sample/scalar/model.onnx "http://server.11math.com/static/onnxruntime-server/sample/scalar.onnx"
