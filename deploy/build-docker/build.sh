#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source deploy/build-docker/VERSION
MODE=prepare
TARGET=all
RECEIPT=''
for arg in "$@"; do
    case "$arg" in
        --prepare) MODE=prepare ;;
        --publish=*) MODE=publish; RECEIPT="${arg#*=}" ;;
        --target=cpu) TARGET=cpu ;;
        --target=cuda) TARGET=cuda ;;
        *) echo "usage: build.sh [--prepare] [--target=cpu|cuda] | --publish=<receipt.json>" >&2; exit 1 ;;
    esac
done
test -z "$(git status --porcelain)" || { echo 'commit source changes before building release images' >&2; exit 1; }
SHA=$(git rev-parse HEAD)
TREE=$(git rev-parse HEAD^{tree})
STATE="deploy/build-docker/out/release-${VERSION}-${SHA}"
mkdir -p "$STATE"

if [ "$MODE" = publish ]; then
    # Validate all receipt entries before performing any registry mutation.
    python3 -c '
import json, re, sys
r = json.load(open(sys.argv[1]))
if r["tree"] != sys.argv[2] or r["version"] != sys.argv[3] or r["image_prefix"] != sys.argv[4]:
    sys.exit("receipt does not match the current release tree/version/repository")
if not r["images"] or len({i["variant"] for i in r["images"]}) != len(r["images"]):
    sys.exit("empty or duplicate receipt images")
for i in r["images"]:
    if i["variant"] not in ("linux-cpu", "linux-cuda12", "linux-cuda13") or not re.fullmatch("sha256:[a-f0-9]{64}", i["digest"]):
        sys.exit("invalid receipt image")
    print(i["variant"], i["digest"])
' "$RECEIPT" "$TREE" "$VERSION" "$IMAGE_PREFIX" > "$STATE/publish-list"
    # Network/auth failures must not be treated as an absent tag.
    while read -r variant digest; do
        final="$IMAGE_PREFIX:$VERSION-$variant"
        if current=$(docker buildx imagetools inspect "$final" --format '{{.Manifest.Digest}}' 2> "$STATE/inspect-error"); then
            [ "$current" = "$digest" ] || { echo "refusing to overwrite $final ($current != $digest)" >&2; exit 1; }
        elif ! grep -qi 'manifest unknown' "$STATE/inspect-error" &&
             ! grep -qF "$final: not found" "$STATE/inspect-error"; then
            cat "$STATE/inspect-error" >&2; exit 1
        fi
    done < "$STATE/publish-list"
    while read -r variant digest; do
        final="$IMAGE_PREFIX:$VERSION-$variant"
        # --prefer-index=false preserves a single-platform manifest digest.
        docker buildx imagetools create --prefer-index=false --tag "$final" "$IMAGE_PREFIX@$digest"
        current=$(docker buildx imagetools inspect "$final" --format '{{.Manifest.Digest}}')
        [ "$current" = "$digest" ] || { echo "published digest mismatch for $final" >&2; exit 1; }
    done < "$STATE/publish-list"
    exit 0
fi

VARIANTS=()
[ "$TARGET" = cuda ] || VARIANTS+=(linux-cpu)
[ "$TARGET" = cpu ] || VARIANTS+=(linux-cuda12 linux-cuda13)
# Complete all local amd64 tests before pushing even a candidate tag.
for variant in "${VARIANTS[@]}"; do
    image="$IMAGE_PREFIX:candidate-$VERSION-$SHA-$variant"
    docker buildx build --platform linux/amd64 --label "org.opencontainers.image.revision=$SHA" \
        -t "$image" \
        -f "deploy/build-docker/$variant.dockerfile" --load .
    cuda=0; [ "$variant" = linux-cpu ] || cuda=1
    bash deploy/build-docker/docker-image-test.sh "$image" "$cuda"
    docker image inspect "$image" --format '{{.Id}}' > "$STATE/$variant.tested-config"
done
printf '%s\n' "$TREE" > "$STATE/tree"
printf '%s\n' "$SHA" > "$STATE/sha"
printf '%s\n' "$VERSION" > "$STATE/version"
printf '%s\n' "$IMAGE_PREFIX" > "$STATE/image-prefix"
for variant in "${VARIANTS[@]}"; do
    image="$IMAGE_PREFIX:candidate-$VERSION-$SHA-$variant"
    platforms=linux/amd64
    [ "$variant" != linux-cpu ] || platforms=linux/amd64,linux/arm64
    # Separate candidate namespace: partial uploads never expose final release tags.
    docker buildx build --platform "$platforms" --label "org.opencontainers.image.revision=$SHA" \
        -t "$image" \
        -f "deploy/build-docker/$variant.dockerfile" --push .
    docker buildx imagetools inspect "$image" --format '{{.Manifest.Digest}}' > "$STATE/$variant.digest"
    docker buildx imagetools inspect "$image" --raw > "$STATE/$variant.manifest.json"
    amd64=$(python3 -c '
import json, sys
m=json.load(open(sys.argv[1]))
if "config" in m:
    print("config", m["config"]["digest"])
else:
    matches=[d for d in m["manifests"] if d.get("platform", {}).get("architecture") == "amd64" and d["platform"].get("os") == "linux"]
    if len(matches) != 1: sys.exit("expected exactly one amd64 manifest")
    print("manifest", matches[0]["digest"])
' "$STATE/$variant.manifest.json")
    read -r kind digest <<< "$amd64"
    if [ "$kind" = manifest ]; then
        config=$(docker buildx imagetools inspect "$IMAGE_PREFIX@$digest" --raw | python3 -c 'import json,sys; print(json.load(sys.stdin)["config"]["digest"])')
    else
        config="$digest"
    fi
    [ "$config" = "$(< "$STATE/$variant.tested-config")" ] || {
        echo "candidate $variant differs from the tested amd64 image; do not publish" >&2; exit 1;
    }
done
test "$(git rev-parse HEAD)" = "$SHA" && test -z "$(git status --porcelain)" || {
    echo 'source changed during candidate preparation; discard this attempt and rerun CI' >&2; exit 1;
}
python3 -c '
import json, pathlib, re, sys
p = pathlib.Path(sys.argv[1])
r = {k: (p / f).read_text().strip() for k, f in (("tree", "tree"), ("sha", "sha"), ("version", "version"), ("image_prefix", "image-prefix"))}
r["images"] = []
for variant in sys.argv[2:]:
    digest = (p / (variant + ".digest")).read_text().strip()
    if not re.fullmatch("sha256:[a-f0-9]{64}", digest):
        sys.exit("invalid candidate digest")
    r["images"].append({"variant": variant, "digest": digest, "tested_platforms": ["linux/amd64"]})
print(json.dumps(r, indent=2))
' "$STATE" "${VARIANTS[@]}" > "$STATE/receipt.json"
echo "All selected images passed amd64 tests. Candidate receipt: $STATE/receipt.json"
echo 'After merge, publish these digests using build.sh --publish=<receipt.json>.'
