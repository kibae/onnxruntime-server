#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

echo
echo "Select onnxruntime version to download:"
RAW_LIST=$(curl -s -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/microsoft/onnxruntime/releases/latest \
  | grep browser_download_url \
  | grep -E "onnxruntime-linux-x64-([.0-9]+)tgz" \
  | awk '{print $2}' \
  | tr -d '"')

item=${RAW_LIST[0]}

FILENAME=$(basename "$item")

echo
echo "Downloading $item"
echo

wget -q "$item"

sudo mkdir -p /usr/local/onnxruntime
sudo tar vzxf "$FILENAME" -C /usr/local/onnxruntime --strip-components=1
sudo sh -c 'echo "/usr/local/onnxruntime/lib" > /etc/ld.so.conf.d/onnxruntime.conf'
sudo ldconfig

rm -f "$FILENAME"

echo
echo "Done"
echo
