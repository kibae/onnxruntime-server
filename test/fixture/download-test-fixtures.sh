#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

curl -o sample/1/model.onnx "https://kibae.blob.core.windows.net/static/model1.onnx"
curl -o sample/2/model.onnx "https://kibae.blob.core.windows.net/static/model2.onnx"
