#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source deploy/build-docker/VERSION

LOCAL=0
DRY_RUN=0
TARGET=all
for arg in "$@"; do
    case "$arg" in
        --local) LOCAL=1 ;;
        --dry-run) DRY_RUN=1 ;;
        --target=cpu) TARGET=cpu ;;
        --target=cuda) TARGET=cuda ;;
        --help|-h)
            echo 'usage: build.sh [--local] [--dry-run] [--target=cpu|cuda]'
            echo 'Default: build, test and push release tags. --local: build and test only.'
            echo '--dry-run: print tags and commands without executing them.'
            exit 0 ;;
        *) echo "unknown argument: $arg (see --help)" >&2; exit 1 ;;
    esac
done

VARIANTS=()
[ "$TARGET" = cuda ] || VARIANTS+=(linux-cpu)
[ "$TARGET" = cpu ] || VARIANTS+=(linux-cuda12 linux-cuda13)

if [ "$LOCAL" = 1 ]; then
    echo 'Images to build and test locally (no push):'
else
    echo 'Images to build, test and push:'
fi
for variant in "${VARIANTS[@]}"; do
    printf '  %s:%s-%s\n' "$IMAGE_PREFIX" "$VERSION" "$variant"
done

run() {
    printf '+ '; printf '%q ' "$@"; printf '\n'
    if [ "$DRY_RUN" = 0 ]; then "$@"; fi
}

build_image() {
    local variant="$1" platforms="$2" output="$3" digest=""
    local context=()
    if [ "$ORT_VERSION" = 1.32.0 ]; then
        case "$variant" in
            linux-cuda12) digest=6248d0503bc47e14e994beeaec1944fd7b49886527003c7eaa32e620e882c09d ;;
            linux-cuda13) digest=47994d7c804844830fb35d65025ab5aa0020e96f3d972c61149375bbf1ef1294 ;;
        esac
    fi
    if [ -n "$digest" ]; then
        context=(--build-context "ort-sdk=docker-image://kibaes/onnxruntime-server@sha256:$digest")
    fi
    run docker buildx build "${context[@]}" --platform "$platforms" -t "$IMAGE_PREFIX:$VERSION-$variant" \
        -f "deploy/build-docker/$variant.dockerfile" "$output" .
}

# Complete every selected local test before starting any upload.
for variant in "${VARIANTS[@]}"; do
    image="$IMAGE_PREFIX:$VERSION-$variant"
    build_image "$variant" linux/amd64 --load
    cuda=0; [ "$variant" = linux-cpu ] || cuda=1
    run bash deploy/build-docker/docker-image-test.sh "$image" "$cuda"
done

[ "$LOCAL" = 0 ] || exit 0
for variant in "${VARIANTS[@]}"; do
    image="$IMAGE_PREFIX:$VERSION-$variant"
    platforms=linux/amd64
    [ "$variant" != linux-cpu ] || platforms=linux/amd64,linux/arm64
    build_image "$variant" "$platforms" --push
done
