#!/usr/bin/env bash
set -euo pipefail
export ORT_JSON_PYTHON=python
source "$(dirname "$0")/../../deploy/build-docker/release-assets.sh"
ORT_TARGET=$(ort_version "${1:-}")
META=$(ort_asset "$ORT_TARGET" "onnxruntime-win-x64-${ORT_TARGET}.zip" \
    "onnxruntime-win-x64-gpu_cuda12-${ORT_TARGET}.zip" "onnxruntime-win-x64-gpu-${ORT_TARGET}.zip")
IFS=$'\t' read -r NAME URL DIGEST <<< "$META"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
ort_download "$URL" "$TMP/ort.zip" "$DIGEST"
unzip -q "$TMP/ort.zip" -d "$TMP"
ROOT="$TMP/${NAME%.zip}"
test -f "$ROOT/include/onnxruntime_cxx_api.h"
test -f "$ROOT/lib/onnxruntime.dll"
test "$(tr -d '[:space:]' < "$ROOT/VERSION_NUMBER")" = "$ORT_TARGET"
test ! -e C:/msys64/usr/local/onnxruntime || { echo 'ONNX Runtime destination already exists' >&2; exit 1; }
mkdir -p C:/msys64/usr/local
mv "$ROOT" C:/msys64/usr/local/onnxruntime
