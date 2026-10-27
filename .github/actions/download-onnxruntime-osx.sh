#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../../deploy/build-docker/release-assets.sh"
ORT_TARGET=$(ort_version "${1:-}")
META=$(ort_asset "$ORT_TARGET" "onnxruntime-osx-arm64-${ORT_TARGET}.tgz")
IFS=$'\t' read -r NAME URL DIGEST <<< "$META"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
ort_download "$URL" "$TMP/ort.tgz" "$DIGEST"
# Preserve the archive root: upstream archives may include a leading ./.
tar -xzf "$TMP/ort.tgz" -C "$TMP"
RUNTIME="$TMP/onnxruntime-osx-arm64-${ORT_TARGET}"
test -f "$RUNTIME/include/onnxruntime_cxx_api.h"
test -f "$RUNTIME/lib/libonnxruntime.dylib"
test ! -e /usr/local/onnxruntime || { echo 'ONNX Runtime destination already exists' >&2; exit 1; }
sudo mv "$RUNTIME" /usr/local/onnxruntime
