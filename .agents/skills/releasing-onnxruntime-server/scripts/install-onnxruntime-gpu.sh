#!/usr/bin/env bash
set -euo pipefail
# Use a fresh local directory; never replace the system runtime.
VERSION="${1:?usage: install-onnxruntime-gpu.sh <version> <new-directory>}"
DEST="${2:?usage: install-onnxruntime-gpu.sh <version> <new-directory>}"
REPO=$(git rev-parse --show-toplevel)
source "$REPO/deploy/build-docker/release-assets.sh"
VERSION=$(ort_version "$VERSION")
test ! -e "$DEST" && test ! -L "$DEST" || { echo "destination already exists: $DEST" >&2; exit 1; }
mkdir -p "$(dirname "$DEST")"
STAGE=$(mktemp -d "${DEST}.partial.XXXXXX")
trap 'echo "installation artifacts preserved in $STAGE" >&2' ERR
META=$(ort_asset "$VERSION" "onnxruntime-linux-x64-gpu_cuda12-${VERSION}.tgz" \
    "onnxruntime-linux-x64-gpu-${VERSION}.tgz")
IFS=$'\t' read -r NAME URL DIGEST <<< "$META"
ort_download "$URL" "$STAGE/ort.tgz" "$DIGEST"
tar -xzf "$STAGE/ort.tgz" -C "$STAGE"
ROOT="$STAGE/${NAME%.tgz}"
test -f "$ROOT/include/onnxruntime_cxx_api.h"
test -f "$ROOT/lib/libonnxruntime.so"
test -f "$ROOT/lib/libonnxruntime_providers_cuda.so"
test "$(tr -d '[:space:]' < "$ROOT/VERSION_NUMBER")" = "$VERSION"
mv -T "$ROOT" "$DEST"
echo "installed ONNX Runtime $VERSION at $DEST; archive preserved in $STAGE"
