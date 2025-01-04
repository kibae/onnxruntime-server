#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

echo
echo "Select onnxruntime version to download:"
RAW_LIST=$(curl -s -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/microsoft/onnxruntime/releases/latest \
  | grep browser_download_url \
  | grep -E "onnxruntime-win-x64-([.0-9]+)zip" \
  | awk '{print $2}' \
  | tr -d '"')

item=${RAW_LIST[0]}

FILENAME=$(basename "$item")

echo
echo "Downloading $item"
echo

curl --output "$FILENAME" "$item"

mkdir -p C:/msys64/usr/local/onnxruntime
unzip -q "$FILENAME" -d C:/msys64/usr/local/onnxruntime
ls -al C:/msys64/usr/local/onnxruntime

rm -f "$FILENAME"

echo
echo "Done"
echo
