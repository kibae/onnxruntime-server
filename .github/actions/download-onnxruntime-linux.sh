#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

echo
echo "Select onnxruntime version to download:"
AUTH_HEADER=""
if [ -n "$GITHUB_TOKEN" ]; then
  AUTH_HEADER="-H \"Authorization: Bearer $GITHUB_TOKEN\""
fi

RAW_LIST=$(eval curl -s -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  $AUTH_HEADER \
  https://api.github.com/repos/microsoft/onnxruntime/releases/latest \
  | grep browser_download_url \
  | grep -E "onnxruntime-linux-x64-([.0-9]+)tgz" \
  | awk '{print $2}' \
  | tr -d '"')

item=${RAW_LIST[0]}

if [ -z "$item" ]; then
  echo "Error: Could not find onnxruntime download link (regex matched no asset; the GitHub API may be rate-limiting unauthenticated requests on this runner — pass GITHUB_TOKEN from the workflow if so)." >&2
  exit 1
fi

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
