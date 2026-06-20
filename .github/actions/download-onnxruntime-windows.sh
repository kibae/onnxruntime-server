#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

echo
echo "Select onnxruntime version to download:"
# onnxruntime 1.27.0 dropped the plain win-x64 (CPU-only) archive and ships only gpu_cuda{12,13}
# variants. The cuda12 build links the same headers and runs on CPU when no GPU is available, so
# the CI builder uses it.
AUTH_HEADER=()
if [ -n "$GITHUB_TOKEN" ]; then
  AUTH_HEADER=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi
RAW_LIST=$(curl -s -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  "${AUTH_HEADER[@]}" \
  https://api.github.com/repos/microsoft/onnxruntime/releases/latest \
  | grep browser_download_url \
  | grep -E "onnxruntime-win-x64-gpu_cuda12-([.0-9]+)zip" \
  | awk '{print $2}' \
  | tr -d '"')

item=${RAW_LIST[0]}

FILENAME=$(basename "$item")
DIRNAME="${FILENAME%.zip}"

echo
echo "Downloading $item"
echo

curl -q --output "$FILENAME" -L "$item"

unzip "$FILENAME"
mv ${DIRNAME} C:/msys64/usr/local/onnxruntime
ls -al C:/msys64/usr/local/onnxruntime

rm -f "$FILENAME"

echo
echo "Done"
echo
