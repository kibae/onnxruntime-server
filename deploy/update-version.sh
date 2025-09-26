#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit

FROM_VERSION=$1
TO_VERSION=$2

if [ -z "$FROM_VERSION" ] || [ -z "$TO_VERSION" ]; then
    echo "Usage: $0 <from_version> <to_version>"
    exit 1
fi

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
    sed -i '' "s/${FROM_VERSION}/${TO_VERSION}/g" "$file"
done

echo "Done"
echo

echo "Update docker hub page"
echo "https://hub.docker.com/repository/docker/kibaes/onnxruntime-server/general"

