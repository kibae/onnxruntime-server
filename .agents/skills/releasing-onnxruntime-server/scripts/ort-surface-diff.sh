#!/usr/bin/env bash
set -euo pipefail
FROM="${1:?usage: ort-surface-diff.sh <from> <to> [outdir]}"
TO="${2:?usage: ort-surface-diff.sh <from> <to> [outdir]}"
[[ "$FROM" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ && "$TO" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 1
OUT="${3:-$(mktemp -d)}"
mkdir -p "$OUT"
HEADER_PATH=include/onnxruntime/core/session
fetch_headers() {
    local ver="$1" dir="$OUT/$1" name
    test ! -e "$dir" || { echo "use a fresh output directory: $dir exists" >&2; return 1; }
    mkdir "$dir"
    gh api "repos/microsoft/onnxruntime/contents/$HEADER_PATH?ref=v$ver" \
        --jq '.[] | select(.type == "file" and (.name | test("\\.(h|inc)$"))) | .name' > "$OUT/$ver.headers"
    test -s "$OUT/$ver.headers"
    while IFS= read -r name; do
        [[ "$name" != */* && "$name" != .* ]] || return 1
        gh api "repos/microsoft/onnxruntime/contents/$HEADER_PATH/$name?ref=v$ver" \
            -H 'Accept: application/vnd.github.raw' > "$dir/$name"
        test -s "$dir/$name"
    done < "$OUT/$ver.headers"
    python3 - "$dir" <<'PY'
import pathlib, re, sys
p = pathlib.Path(sys.argv[1])
headers = "\n".join(f.read_text() for f in sorted(p.iterdir()))
symbols = sorted(set(re.findall(r"\b(?:ONNX_TENSOR_ELEMENT_DATA_TYPE_[A-Z0-9_]+|ORT_[A-Z0-9_]+|kOrt[A-Za-z0-9_]+)\b", headers)))
cxx = (p / "onnxruntime_cxx_api.h").read_text()
block = re.search(r"struct\s+SessionOptionsImpl\b[^\{]*\{(.*?)^\};", cxx, re.M | re.S)
if not block:
    sys.exit("cannot identify SessionOptionsImpl; update the extractor before releasing")
body = re.sub(r"//[^\n]*|/\*.*?\*/", "", block[1], flags=re.S)
setters = sorted(set(re.findall(r"\b([A-Z][A-Za-z0-9_]*)\s*\(", body)))
if not symbols or "SetGraphOptimizationLevel" not in setters:
    sys.exit("header extraction did not produce the expected API surface")
for kind, values in (("symbols", symbols), ("setters", setters)):
    (p.parent / (p.name + "." + kind)).write_text("\n".join(values) + "\n")
PY
}
checked_diff() {
    local code=0
    diff "$@" || code=$?
    [ "$code" -le 1 ] || return "$code"
}
fetch_headers "$FROM"
fetch_headers "$TO"
for kind in headers symbols setters; do
    checked_diff "$OUT/$FROM.$kind" "$OUT/$TO.$kind" > "$OUT/$kind.diff"
done
checked_diff -ru "$OUT/$FROM" "$OUT/$TO" > "$OUT/api.diff"
# Capture the producer status rather than hiding it in process substitution.
gh api repos/microsoft/onnxruntime/releases --paginate --jq '.[].tag_name' > "$OUT/release-tags"
python3 - "$FROM" "$TO" "$OUT" <<'PY'
import pathlib, re, sys
parse = lambda v: tuple(map(int, v.split(".")))
lo, hi = map(parse, sys.argv[1:3])
if lo >= hi:
    sys.exit("surface review requires an older baseline")
p = pathlib.Path(sys.argv[3])
versions = sorted({t[1:] for t in (p / "release-tags").read_text().splitlines() if re.fullmatch(r"v\d+\.\d+\.\d+", t)}, key=parse)
selected = [v for v in versions if lo < parse(v) <= hi]
if sys.argv[2] not in selected:
    sys.exit("target release is absent from the release listing; review is incomplete")
(p / "notes-list").write_text("\n".join(selected) + "\n")
PY
while IFS= read -r version; do
    gh api "repos/microsoft/onnxruntime/releases/tags/v$version" --jq '.body // ""' > "$OUT/notes-$version.md"
done < "$OUT/notes-list"
for kind in headers symbols setters; do
    echo "== $kind delta"
    if [ -s "$OUT/$kind.diff" ]; then cat "$OUT/$kind.diff"; else echo '(none)'; fi
done
echo "Complete review artifacts: $OUT (including api.diff and notes-*.md)"
