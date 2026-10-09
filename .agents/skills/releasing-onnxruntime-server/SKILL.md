---
name: releasing-onnxruntime-server
description: Release onnxruntime-server against a specified or latest ONNX Runtime version, including upstream API review, CUDA alignment, CI, Docker images, PR merge, tag and GitHub release. Use when asked to release or ship this repository; reviewing or importing this skill alone does not authorize a release.
---

# Releasing onnxruntime-server

For a complete release request, prepare changes, open the PR, fix CI, test/upload
Docker candidates, merge, promote recorded digests, tag the merge commit and publish
notes. Honor narrower requested scope. Reviewing/importing/editing this skill does
not run a release. Preserve the original explicit-only invocation policy.

Use the version in the user's request as `TO_VERSION`, or resolve upstream
`microsoft/onnxruntime/releases/latest` once with `gh api`. This is conversational
input, not shell `$1`. Pass explicit variables to each shell call and record state
under ignored `.release-work/`. Never resolve `latest` again in CI or Docker.

Resolve `SKILL_DIR` from this file's absolute path. Run repo commands from its root.
Invoke helpers with `bash`/`python3`; executable file mode is unnecessary. Read
[references/upstream-review.md](references/upstream-review.md) for API/CUDA review,
and [references/release-notes.md](references/release-notes.md) when writing notes.

## Invariants

- `deploy/build-docker/VERSION` distinguishes server `VERSION` and upstream
  `ORT_VERSION`. Install, CI, Docker and the version unit test must agree on ORT.
- Test the committed source used for images. Fixes invalidate CI and candidate
  receipts: commit, push and rerun the gates on the new SHA.
- Finish all selected image tests before candidate uploads. Promote recorded
  digests after merge without rebuilding. Multiple registry tags are not atomic;
  retry a partial promotion using the same receipt, never a different build.
- Bind checks/merge to a specific PR head. Require `main` to be its ancestor; if
  main moves, incorporate it on the release branch and rerun CI/images.
- Tag the PR's actual merge SHA, never whatever main points to later.
- Keep unrelated work and existing system ORT/build directories. Use fresh local
  directories. Report unhandled failures with command/stderr, preserving artifacts.
  Fix CI failures on the same branch with new commits rather than amendments.

## 1. Preflight and versions

Check repository identity, origin URL, current branch, tracked/untracked changes,
and existing artifacts before switching branches. Do not stash or overwrite unrelated
work. Verify `gh auth status`, Docker/buildx, CMake/CTest, Python 3, curl, jq, tar and
NVIDIA GPU availability for CUDA tests. This installer needs Linux x64; do not assume
WSL or driver versions from a previous release.

For a fresh release, fetch origin, fast-forward main and require local main to equal
origin/main. Record `BASE_MAIN` and previous `FROM_VERSION`. Require stable `X.Y.Z`
`TO_VERSION` newer than the previous upstream version; stop without changes when
already released. Create `release/$TO_VERSION` before edits, not after review/build.

Derive `ORT_BASELINE` from the previous project commit's `ORT_VERSION`, not the
developer's installed runtime. For older commits without that field, verify their
`src/test/test_lib_version.cpp` expected runtime and historical release evidence.
Server-only versions like `1.23.2a`/`1.24.4` need an evidenced upstream baseline;
do not guess a nearest lower tag just because it exists.

Create `.release-work/`, then `WORK=$(mktemp -d "$REPO/.release-work/$TO_VERSION.XXXXXX")`.
Record versions, baseline, branch/PR, runtime/build paths, checked head/base, receipt,
merge SHA and publication progress. Do not source untrusted state as shell code.

## 2. Install and review

```bash
bash "$SKILL_DIR/scripts/install-onnxruntime-gpu.sh" "$TO_VERSION" "$WORK/runtime"
bash "$SKILL_DIR/scripts/ort-surface-diff.sh" "$ORT_BASELINE" "$TO_VERSION" "$WORK/surface"
bash "$SKILL_DIR/scripts/ort-cuda-base-images.sh" "$TO_VERSION"
```

Install validates the exact archive into a new path, preserving `/usr/local/onnxruntime`
and ld.so configuration. Use another new path on retry. Changed assets require syncing
the Docker/CI downloaders; distinguish missing assets from API failures.

Read every interval release note and header/symbol/setter delta, plus `api.diff` for
signatures and existing enum values. Implement relevant mirrors/tests; record skipped
findings and revalidated assumptions. Preserve historical version comments when behavior
still holds. Commit meaningful API fixes separately from the version bump.

Read CUDA packaging definitions from immutable `v$TO_VERSION`. Follow build images
and immutable pipeline commits if the tag alone does not establish actual packaging.
Choose the highest compatible patch in upstream's CUDA minor on the established
Ubuntu base, preferring upstream cuDNN. Inspect exact candidate tags by rerunning
the CUDA helper with those tags. Compare complete `NVIDIA_REQUIRE_*` expressions
and library requirements with current published images. Minor compatibility, PTX
and GPU brand constraints matter; a newer toolkit minor alone does not establish
a higher universal driver minimum. Ask about an evidenced support-floor increase
unless the session already authorized it. Do not universalize one WSL measurement.

Update both builder/runtime stages and the base table when needed. Commit base
alignment with sources and before/after compatibility effects. Commit API/base changes
before the version bump, avoiding destructive restores of files they share.

## 3. Version update and local verification

Run `bash deploy/update-version.sh "$FROM_VERSION" "$TO_VERSION" "$TO_VERSION"`.
Inspect the established seven version files, including separately pinned `ORT_VERSION`.
Never stage unrelated files. Use a fresh local build and explicit ORT paths; suppress
pkg-config's system ORT lookup (archive .pc files can have system prefixes):

```bash
mkdir -p "$WORK/no-pkgconfig"
PKG_CONFIG_PATH="" PKG_CONFIG_LIBDIR="$WORK/no-pkgconfig" \
cmake -S . -B "$WORK/build" -DCMAKE_BUILD_TYPE=Debug \
  -DONNX_RUNTIME_INCLUDE_DIRS="$WORK/runtime/include" \
  -DONNX_RUNTIME_LIBRARY="$WORK/runtime/lib/libonnxruntime.so" \
  -DONNX_RUNTIME_LIBRARY_DIRS="$WORK/runtime/lib" \
  -DONNX_RUNTIME_CUDA_LIBRARY="$WORK/runtime/lib/libonnxruntime_providers_cuda.so" \
  -DONNX_RUNTIME_LIBRARIES="$WORK/runtime/lib/libonnxruntime.so;$WORK/runtime/lib/libonnxruntime_providers_cuda.so"
cmake --build "$WORK/build" -j"$(nproc)"
LD_LIBRARY_PATH="$WORK/runtime/lib:${LD_LIBRARY_PATH:-}" \
  ctest --test-dir "$WORK/build" --output-on-failure --no-tests=error
```

Inspect chosen paths and version test; require GTest and actual test discovery.
Account for local CUDA/cuDNN loader paths. A tripwire changing is a review finding,
not grounds to weaken tests blindly. Commit exactly the seven version files as
`ci: release $TO_VERSION`, then push. Docker preparation later tests all images.

## 4. PR and CI gate

Create/update one PR from `release/$TO_VERSION` to main, title `ci: release $TO_VERSION`.
Use a body file with `gh pr create --body-file`. If GraphQL encounters the
Projects-classic error, update through REST using `gh api ... -X PATCH -F body=@<file>`.
Follow the writing reference and tick only completed work.

Set `RELEASE_HEAD=$(git rev-parse HEAD)`, verify it equals the PR's remote head,
fetch origin and require `git merge-base --is-ancestor origin/main "$RELEASE_HEAD"`.
Poll with short waits and keep the user informed:

```bash
python3 "$SKILL_DIR/scripts/check-release-ci.py" kibae/onnxruntime-server "$PR" "$RELEASE_HEAD"
```

The helper checks the latest Linux/Windows/macOS/CodeQL PR workflows for that SHA,
paginates checks on head and synthetic merge commits, checks statuses and requires
mergeability clean. Missing/pending/skipped/stale/failed results are not success.
Investigate, fix, commit and push failures before restarting the gate. Do not loosen
an assertion merely to turn CI green.

## 5. Images, merge and publication

After CI passes, run `bash deploy/build-docker/build.sh --prepare` from a clean
committed tree. All three amd64 images are tested before candidate uploads; uploaded
amd64 configs must match tested configs. Keep the emitted digest receipt. CUDA
sessions must report device ID 0 and inference must complete. The CPU image still
includes amd64+arm64; this workflow adds no arm64 execution gate, as requested.

On failure preserve artifacts. Code fixes require new commits and complete CI/image
validation; retries of unchanged code remain candidate-only. Before merge, rerun CI,
verify the same HEAD and main ancestry, then:

```bash
gh pr merge "$PR" --merge --match-head-commit "$RELEASE_HEAD"
```

Read the PR via REST, require `merged == true`, and save `merge_commit_sha` as
`MERGE_SHA`. Fetch origin, require it to be on origin/main, and compare its tree
with RELEASE_HEAD and the receipt. A mismatch requires validating the resulting
code again before final artifact publication. Use a clean checkout of MERGE_SHA
(isolated if main moved) to run `bash deploy/build-docker/build.sh --publish="$RECEIPT"`.

Promotion checks all final tags first: identical digests are resumable, different
digests stop, auth/network failures are not absent tags. A partial promotion can
leave some final tags visible; record this and retry the same receipt. Once every
final digest matches, create `v$TO_VERSION` at MERGE_SHA and push. On retry compare
local/remote tags first; reuse matches, stop on mismatches, never force-move a tag.

## 6. Notes and handoff

Read the last two release bodies in full and follow the writing reference. Write
and reread a notes file, check external numbers, then publish with
`gh release create "v$TO_VERSION" --verify-tag --title "v$TO_VERSION" --notes-file "$NOTES" --latest`.
If already present, inspect tag/notes and honor the user's scope before any edit;
do not create duplicate releases or overwrite blindly.

Report PR/release URLs, exact tag SHA, image tags/digests, actual test scope and
deliberate omissions. Hand off the manual Docker Hub description update:
https://hub.docker.com/repository/docker/kibaes/onnxruntime-server/general.

## Resume

Inspect branch/PR head and checks, local state, receipt, merge state, local/remote
tag and release first. Reuse consistent completed steps instead of restarting
from main. Changed head/base needs fresh validation; a matching merged tree permits
the original receipt. Failed installs/builds use new local directories; preserve
existing developer runtime/build trees. Surface collection uses fresh output so
incomplete old files cannot look complete.
