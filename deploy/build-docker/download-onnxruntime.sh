#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
source ./release-assets.sh
OS="${1:?usage: download-onnxruntime.sh <os> <arch> [ort-version]}"
ARCH="${2:?usage: download-onnxruntime.sh <os> <arch> [ort-version]}"
ORT_TARGET=$(ort_version "${3:-}")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

if META=$(ort_asset "$ORT_TARGET" "onnxruntime-${OS}-${ARCH}-${ORT_TARGET}.tgz"); then
    IFS=$'\t' read -r NAME URL DIGEST <<< "$META"
    ort_download "$URL" "$TMP/ort.tgz" "$DIGEST"
    tar -xzf "$TMP/ort.tgz" -C "$TMP"
    ROOT="$TMP/${NAME%.tgz}"
    test -f "$ROOT/include/onnxruntime_cxx_api.h"
    test -f "$ROOT/lib/libonnxruntime.so"
    test "$(tr -d '[:space:]' < "$ROOT/VERSION_NUMBER")" = "$ORT_TARGET"
elif [ "$?" = 3 ] && [ "$OS-$ARCH" = linux-aarch64 ]; then
    # Exact-version NuGet fallback for releases without a usable arm64 tgz.
    ort_download "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${ORT_TARGET}/microsoft.ml.onnxruntime.${ORT_TARGET}.nupkg" "$TMP/ort.nupkg"
    unzip -q "$TMP/ort.nupkg" 'runtimes/linux-arm64/native/*' 'build/native/include/*' -d "$TMP/nupkg"
    ROOT="$TMP/runtime"
    mkdir -p "$ROOT/lib" "$ROOT/include"
    cp "$TMP"/nupkg/runtimes/linux-arm64/native/*.so "$ROOT/lib/"
    cp "$TMP"/nupkg/build/native/include/* "$ROOT/include/"
    ln -s libonnxruntime.so "$ROOT/lib/libonnxruntime.so.1"
    printf '%s\n' "$ORT_TARGET" > "$ROOT/VERSION_NUMBER"
else
    echo "could not obtain ONNX Runtime v${ORT_TARGET} for $OS-$ARCH" >&2
    exit 1
fi
test ! -e /usr/local/onnxruntime || { echo 'ONNX Runtime destination already exists' >&2; exit 1; }
mv "$ROOT" /usr/local/onnxruntime
printf '/usr/local/onnxruntime/lib\n' > /etc/ld.so.conf.d/onnxruntime.conf
ldconfig
