#!/usr/bin/env bash
# Shared by CI, Docker builds and the release skill. Source this file with pipefail enabled.
ORT_ASSETS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

ort_version() {
    local requested="${1:-}" VERSION ORT_VERSION
    if [ -z "$requested" ]; then
        source "$ORT_ASSETS_DIR/VERSION"
        requested="${ORT_VERSION:-$VERSION}"
    fi
    [[ "$requested" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
        echo "invalid upstream ONNX Runtime version: $requested" >&2; return 1;
    }
    printf '%s\n' "$requested"
}

# Print name, URL and optional sha256 digest, tab-separated.
# Exit 3 means no matching asset; API and JSON errors remain failures.
ort_asset() {
    local version="$1"; shift
    local auth=() parser="${ORT_JSON_PYTHON:-python3}"
    if [ -n "${GITHUB_TOKEN:-}" ]; then auth=(-H "Authorization: Bearer $GITHUB_TOKEN"); fi
    curl -fsSL "${auth[@]}" -H 'Accept: application/vnd.github+json' \
        "https://api.github.com/repos/microsoft/onnxruntime/releases/tags/v${version}" \
        | "$parser" -c '
import json, sys
release = json.load(sys.stdin)
if release.get("tag_name") != "v" + sys.argv[1]:
    sys.exit("release tag does not match the requested version")
for name in sys.argv[2:]:
    assets = [a for a in release["assets"] if a["name"] == name]
    if len(assets) > 1:
        sys.exit("duplicate release asset: " + name)
    if assets:
        a = assets[0]
        print("\t".join((name, a["browser_download_url"], a.get("digest") or "-")))
        sys.exit(0)
sys.exit(3)
' "$version" "$@"
}

ort_download() {
    local url="$1" destination="$2" digest="${3:--}" actual
    curl -fsSL -o "$destination" "$url" || return
    if [ "$digest" != '-' ]; then
        [[ "$digest" =~ ^sha256:[a-fA-F0-9]{64}$ ]] || {
            echo "unsupported asset digest: $digest" >&2; return 1;
        }
        if command -v sha256sum >/dev/null; then
            actual=$(sha256sum "$destination") || return
        else
            actual=$(shasum -a 256 "$destination") || return
        fi
        [ "${actual%% *}" = "${digest#sha256:}" ] || {
            echo 'ONNX Runtime archive checksum mismatch' >&2; return 1;
        }
    fi
}

# The 1.32.0 GitHub release assets are unavailable. Install the same release
# from Microsoft's CPU NuGet package, including the native loader's library name.
ort_nuget_cpu_1320() {
    local rid="$1" work="$2" sdk="$2/runtime"
    local pkg=microsoft.ml.onnxruntime
    ort_download "https://api.nuget.org/v3-flatcontainer/$pkg/1.32.0/$pkg.1.32.0.nupkg" "$work/ort.nupkg" \
        sha256:a9150bc2992cbbb6722070da9910e1ffa989a8bfc67bdd5bb8e745ca755641db
    mkdir -p "$sdk/include" "$sdk/lib"
    unzip -qj "$work/ort.nupkg" 'build/native/include/*' -d "$sdk/include"
    unzip -qj "$work/ort.nupkg" "runtimes/$rid/native/*" -d "$sdk/lib"
    unzip -qj "$work/ort.nupkg" LICENSE ThirdPartyNotices.txt Privacy.md -d "$sdk"
    case "$rid" in
        linux-*) ln -s libonnxruntime.so "$sdk/lib/libonnxruntime.so.1" ;;
        osx-arm64) ln -s libonnxruntime.dylib "$sdk/lib/libonnxruntime.1.dylib" ;;
    esac
    echo 1.32.0 > "$sdk/VERSION_NUMBER"
}
