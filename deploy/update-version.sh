#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.." || exit

FROM_VERSION=${1:-}
TO_VERSION=${2:-}
UPSTREAM_VERSION=${3:-$TO_VERSION}

if [ -z "$FROM_VERSION" ] || [ -z "$TO_VERSION" ]; then
    echo "Usage: $0 <from_version> <to_version> [upstream_version]"
    exit 1
fi
[[ "$FROM_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+[a-zA-Z0-9.+-]*$ ]] || exit 1
[[ "$TO_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+[a-zA-Z0-9.+-]*$ ]] || exit 1
[[ "$UPSTREAM_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 1
FROM_PATTERN=$(printf '%s' "$FROM_VERSION" | sed 's/[.[\*^$\\]/\\&/g')

FILES=(
    "README.md"
    "docs/docker.md"
    "docs/swagger/openapi.yaml"
    "deploy/build-docker/VERSION"
    "deploy/build-docker/docker-compose.yaml"
    "deploy/build-docker/README.md"
    "src/test/test_lib_version.cpp"
    )

pwd

for file in "${FILES[@]}"
do
    echo "Updating $file"
    sed -i "s/${FROM_PATTERN}/${TO_VERSION}/g" "$file"
done
if grep -q '^export ORT_VERSION=' deploy/build-docker/VERSION; then
    sed -i "s/^export ORT_VERSION=.*/export ORT_VERSION=${UPSTREAM_VERSION}/" deploy/build-docker/VERSION
else
    printf 'export ORT_VERSION=%s\n' "$UPSTREAM_VERSION" >> deploy/build-docker/VERSION
fi
sed -i -E "s/(onnxruntime_server::onnx::version\(\), \x22)[^\x22]+(\x22)/\1${UPSTREAM_VERSION}\2/" src/test/test_lib_version.cpp

echo "Done"
echo

echo "Update docker hub page"
echo "https://hub.docker.com/repository/docker/kibaes/onnxruntime-server/general"
