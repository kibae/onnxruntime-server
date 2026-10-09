#!/usr/bin/env bash
set -euo pipefail
VER="${1:?usage: ort-cuda-base-images.sh <ort-version> [nvidia/cuda-tag ...]}"
shift
[[ "$VER" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PIPE_DIR=tools/ci_build/github/azure-pipelines
# A missing optional file is different from an API failure.
gh api "repos/microsoft/onnxruntime/contents/$PIPE_DIR?ref=v$VER" --jq '.[].name' > "$TMP/names"
FILES=("$PIPE_DIR/c-api-noopenmp-packaging-pipelines.yml"
       "$PIPE_DIR/templates/common-variables.yml"
       tools/ci_build/github/linux/docker/Dockerfile.manylinux2_28_cuda)
if grep -qx 'c-api-noopenmp-packaging-pipelines-cuda13.yml' "$TMP/names"; then
    FILES+=("$PIPE_DIR/c-api-noopenmp-packaging-pipelines-cuda13.yml")
fi
echo "== upstream build definitions at immutable tag v$VER"
for path in "${FILES[@]}"; do
    echo "-- $path"
    gh api "repos/microsoft/onnxruntime/contents/$path?ref=v$VER" -H 'Accept: application/vnd.github.raw'
    echo
done

# Supply exact candidate tags after reading upstream toolkit/cuDNN versions.
for tag in "$@"; do
    [[ "$tag" =~ ^[0-9]+\.[0-9]+\.[0-9]+-cudnn-(runtime|devel)-ubuntu[0-9]+\.[0-9]+$ ]] || {
        echo "invalid nvidia/cuda tag: $tag" >&2; exit 1;
    }
    token=$(curl -fsS 'https://auth.docker.io/token?service=registry.docker.io&scope=repository:nvidia/cuda:pull' \
        | python3 -c 'import json,sys; print(json.load(sys.stdin)["token"])')
    accept='application/vnd.oci.image.index.v1+json,application/vnd.docker.distribution.manifest.list.v2+json,application/vnd.oci.image.manifest.v1+json,application/vnd.docker.distribution.manifest.v2+json'
    registry_manifest() {
        curl -fsS -H "Authorization: Bearer $token" -H "Accept: $accept" \
            "https://registry-1.docker.io/v2/nvidia/cuda/manifests/$1"
    }
    registry_manifest "$tag" > "$TMP/manifest.json"
    digest=$(python3 - "$TMP/manifest.json" <<'PY'
import json, sys
m=json.load(open(sys.argv[1]))
if "config" in m:
    print("config", m["config"]["digest"])
else:
    candidates=[d for d in m["manifests"] if d.get("platform", {}).get("os") == "linux" and d["platform"].get("architecture") == "amd64"]
    if len(candidates) != 1: sys.exit("expected one linux/amd64 manifest")
    print("manifest", candidates[0]["digest"])
PY
)
    read -r kind value <<< "$digest"
    if [ "$kind" = manifest ]; then
        value=$(registry_manifest "$value" | python3 -c 'import json,sys; print(json.load(sys.stdin)["config"]["digest"])')
    fi
    echo "== nvidia/cuda:$tag (linux/amd64)"
    curl -fsSL -H "Authorization: Bearer $token" "https://registry-1.docker.io/v2/nvidia/cuda/blobs/$value" \
        | python3 -c '
import json, sys
env = dict(e.split("=", 1) for e in json.load(sys.stdin)["config"]["Env"] if "=" in e)
for k in ("CUDA_VERSION", "NV_CUDNN_VERSION"):
    print(k + "=" + env.get(k, "(not declared)"))
for k, v in sorted(env.items()):
    if k.startswith("NVIDIA_REQUIRE_"):
        print(k + "=" + v)
'
done
echo 'Compare the FULL NVIDIA_REQUIRE_* expressions: spaces mean OR, commas mean AND.'
echo 'Distinguish toolkit-bundled drivers, minor compatibility, and measured host requirements.'
echo 'Check https://docs.nvidia.com/deploy/cuda-compatibility/minor-version-compatibility.html'
echo 'A WSL failure does not establish a universal incompatibility or guarantee for other hosts.'
