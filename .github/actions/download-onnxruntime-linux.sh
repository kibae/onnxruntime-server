#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../../deploy/build-docker/release-assets.sh"
ORT_TARGET=$(ort_version "${1:-}")
META=$(ort_asset "$ORT_TARGET" "onnxruntime-linux-x64-${ORT_TARGET}.tgz")
IFS=$'\t' read -r NAME URL DIGEST <<< "$META"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
ort_download "$URL" "$TMP/ort.tgz" "$DIGEST"
tar -xzf "$TMP/ort.tgz" -C "$TMP"
ROOT="$TMP/${NAME%.tgz}"
test -f "$ROOT/include/onnxruntime_cxx_api.h"
test -f "$ROOT/lib/libonnxruntime.so"
test "$(tr -d '[:space:]' < "$ROOT/VERSION_NUMBER")" = "$ORT_TARGET"
test ! -e /usr/local/onnxruntime || { echo 'ONNX Runtime destination already exists' >&2; exit 1; }
sudo mv "$ROOT" /usr/local/onnxruntime
printf '/usr/local/onnxruntime/lib\n' | sudo tee /etc/ld.so.conf.d/onnxruntime.conf >/dev/null
sudo ldconfig
