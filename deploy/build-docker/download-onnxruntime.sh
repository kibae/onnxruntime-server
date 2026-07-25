#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

OS=$1
ARCH=$2

RELEASE=$(curl -s -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/microsoft/onnxruntime/releases/latest)

echo
echo "Select onnxruntime version to download:"
RAW_LIST=$(echo "$RELEASE" \
  | grep browser_download_url \
  | grep -E "onnxruntime-${OS}-${ARCH}-([.0-9]+)tgz" \
  | awk '{print $2}' \
  | tr -d '"' \
  | head -n 1)

item=${RAW_LIST[0]}

mkdir -p /usr/local/onnxruntime

if [ -n "$item" ]; then
  FILENAME=$(basename "$item")

  echo
  echo "Downloading $item"
  echo

  wget -q "$item" || exit 1
  tar vzxf "$FILENAME" -C /usr/local/onnxruntime --strip-components=1 || exit 1
  rm -f "$FILENAME"
elif [ "${OS}-${ARCH}" = "linux-aarch64" ]; then
  # onnxruntime 1.28.0 uploaded the SBOM folder as onnxruntime-linux-aarch64.zip in place of the
  # onnxruntime-linux-aarch64-<version>.tgz its own manifest lists, so the release carries no
  # usable aarch64 archive. The Microsoft.ML.OnnxRuntime nupkg ships that same CPU build under
  # runtimes/linux-arm64 with an identical header set, so fall back to it until the release asset
  # is fixed. The tgz branch above takes over again as soon as it reappears.
  VERSION=$(echo "$RELEASE" | grep -m 1 '"tag_name"' | awk -F'"' '{print $4}' | sed 's/^v//')
  if [ -z "$VERSION" ]; then
    echo "Error: could not resolve the onnxruntime release version for the aarch64 fallback." >&2
    exit 1
  fi

  NUPKG="microsoft.ml.onnxruntime.${VERSION}.nupkg"

  echo
  echo "Downloading $NUPKG (no aarch64 archive in release v${VERSION})"
  echo

  wget -q -O "$NUPKG" \
    "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${VERSION}/${NUPKG}" || exit 1
  unzip -q -o "$NUPKG" 'runtimes/linux-arm64/native/*' 'build/native/include/*' -d nupkg || exit 1

  mkdir -p /usr/local/onnxruntime/lib /usr/local/onnxruntime/include
  cp nupkg/runtimes/linux-arm64/native/*.so /usr/local/onnxruntime/lib/ || exit 1
  cp nupkg/build/native/include/* /usr/local/onnxruntime/include/ || exit 1

  # The nupkg ships the library flat as libonnxruntime.so, but its SONAME is libonnxruntime.so.1,
  # which is what the loader will look for at runtime. The tgz layout carries that link already;
  # create it here rather than leaving it to the builder-stage ldconfig, because the target stage
  # only copies this directory and never runs ldconfig itself.
  ln -sf libonnxruntime.so /usr/local/onnxruntime/lib/libonnxruntime.so.1 || exit 1

  rm -rf "$NUPKG" nupkg
else
  echo "Error: no onnxruntime archive matched onnxruntime-${OS}-${ARCH}-<version>.tgz." >&2
  exit 1
fi

sh -c 'echo "/usr/local/onnxruntime/lib" > /etc/ld.so.conf.d/onnxruntime.conf'
ldconfig

echo
echo "Done"
echo
