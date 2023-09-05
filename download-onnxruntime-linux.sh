#!/usr/bin/env bash

cd "$(dirname "$0")" || exit
mkdir -p external/onnxruntime || exit

echo
echo "Select onnxruntime version to download:"
RAW_LIST=$(curl -s -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/microsoft/onnxruntime/releases/latest \
  | grep tgz \
  | grep linux \
  | grep browser_download_url \
  | awk '{print $2}' \
  | tr -d '"')

select item in $RAW_LIST; do
  FILENAME=$(basename "$item")
  LIB_DIR=$(pg_config --libdir)

  rm -f "$FILENAME"

  echo
  echo "Downloading $item"
  echo

  wget "$item"

  echo
  echo "Extracting $FILENAME to external/onnxruntime"
  echo

  sudo mkdir -p /usr/local/onnxruntime
  sudo tar vzxf "$FILENAME" -C /usr/local/onnxruntime --strip-components=1
  sudo sh -c 'echo "/usr/local/onnxruntime/lib" > /etc/ld.so.conf.d/onnxruntime.conf'
  sudo ldconfig

  rm -f "$FILENAME"

  echo
  echo "Done"
  echo
  echo
  echo -e "\033[1mTo use this extension, the libonnxruntime library must be installed on the machine running PostgreSQL.\033[0m"
  echo -e "\033[1mCopy the library files from /external/onnxruntime/lib to $LIB_DIR.\033[0m"
  echo
  exit 0
done
