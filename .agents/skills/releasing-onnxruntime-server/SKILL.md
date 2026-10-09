---
name: releasing-onnxruntime-server
description: Release onnxruntime-server against a specified or latest ONNX Runtime version, including upstream API review, CUDA alignment, CI, Docker images, PR merge, tag and GitHub release. Use when asked to release or ship this repository; reviewing or importing this skill alone does not authorize a release.
---

# Releasing onnxruntime-server

For a complete release request, review upstream, implement and verify changes locally,
then commit, open the PR, wait for CI, publish Docker images, merge, tag and publish
notes. Honor narrower requested scope, including review before committing.
Reviewing/importing/editing this skill does not run a release or require redesigning
the project's deployment scripts. Preserve the original explicit-only invocation policy.

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
- Diagnose and verify fixes locally before committing. A failed release step is not
  a reason to push a speculative fix and use the full release cycle as its test.
- Publish images only from the clean committed source that passed CI. Changes to
  that source require CI on the final new SHA before publication.
- Use the established `$IMAGE_PREFIX:$TO_VERSION-linux-{cpu,cuda12,cuda13}` tags.
  Finish all selected image tests before uploads. Inspect the exact tags and commands
  with `build.sh --dry-run` before publishing.
- Bind checks/merge to a specific PR head. Require `main` to be its ancestor; if
  main moves, incorporate it on the release branch and rerun CI/images.
- Tag the PR's actual merge SHA, never whatever main points to later.
- Keep unrelated work and existing system ORT/build directories. Use fresh local
  directories. Report unhandled failures with command/stderr, preserving artifacts.
  Fix CI failures on the same branch with new commits rather than amendments.

## 1. Preflight and versions

Work in the user's existing source checkout. Use `.release-work/` for logs, runtime
archives and build artifacts, not a second source checkout. Check repository identity,
origin URL, branch, tracked/untracked changes and existing artifacts before switching
branches. Do not stash or overwrite unrelated work. Verify `gh auth status`,
Docker/buildx, CMake/CTest, Python 3, curl, jq, tar and NVIDIA GPU availability for CUDA tests. This installer needs Linux x64; do not assume
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
Record versions, baseline, branch/PR, runtime/build paths, checked head/base,
merge SHA, published tags/digests and publication progress. Do not source untrusted
state as shell code.

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

Update both builder/runtime stages and the base table when needed. Record sources
and before/after compatibility effects. After local validation, keep API/base changes
in separate commits ahead of the version-bump commit; use selective staging when
they share a file rather than discarding validated changes.

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
not grounds to weaken tests blindly.

Before committing, preview the Docker commands and run local image validation:

```bash
bash deploy/build-docker/build.sh --local --dry-run
bash deploy/build-docker/build.sh --local
```

This accepts uncommitted changes and builds/tests the three amd64 variants without
uploading. Use `--target=cpu` or `--target=cuda` for focused diagnosis. For a failure,
inspect the actual command, inputs and output, reproduce the specific problem, and
verify the fix locally. When changing CLI integration, exercise the real CLI rather
than relying only on mocks. Do not use a commit/push/CI cycle to discover whether a
speculative fix works. A changed base or Docker script must be exercised locally;
a local amd64 pass does not establish arm64 or cross-platform compatibility.

Once local validation passes, commit the completed API/tooling fixes separately and
exactly the seven version files as `ci: release $TO_VERSION`, then push. If the user
requested review before committing, leave the diff uncommitted and report validation.

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
For a failure, establish the cause and verify a focused fix locally where possible
before committing and restarting the gate. If a failure is specific to a CI platform,
record the evidence and local validation limits. Do not loosen an assertion merely
to turn CI green.

## 5. Images, merge and publication

After CI passes, require a clean tree, the same `RELEASE_HEAD`, and main ancestry.
Preview the exact publication commands, then run the established build/test/push flow:

```bash
bash deploy/build-docker/build.sh --dry-run
bash deploy/build-docker/build.sh
```

The script tests all three amd64 images before uploading the normal version tags.
CUDA sessions must report device ID 0 and inference must complete. CPU publication
includes amd64+arm64; local execution tests cover amd64 only. Verify the published
platforms and record remote digests using structured manifest output:

```bash
docker buildx imagetools inspect "$IMAGE_PREFIX:$TO_VERSION-linux-cpu" --format '{{json .Manifest}}'
```

Repeat the inspection for cuda12/cuda13. Publication across three tags is not atomic;
record successful uploads on failure. Diagnose and validate fixes locally before
committing, rerun CI for changed source, and only then retry publication. An auth or
network failure alone does not require a code change or a new CI run.

Before merge, rerun the CI check and verify the same HEAD and main ancestry, then:

```bash
gh pr merge "$PR" --merge --match-head-commit "$RELEASE_HEAD"
```

Read the PR via REST, require `merged == true`, and save `merge_commit_sha` as
`MERGE_SHA`. Fetch origin, require it to be on origin/main, and compare its tree with
`RELEASE_HEAD`. A mismatch requires reviewing and validating the resulting code
before tagging. Create `v$TO_VERSION` at `MERGE_SHA` and push. On retry compare
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

Inspect branch/PR head and checks, local state, published image tags, merge state,
local/remote git tag and release first. Reuse consistent completed steps. Keep local
diagnosis separate from committing and publication; changed head/base needs fresh
validation before publication. Preserve existing developer runtime/build trees.
Surface collection uses fresh output so incomplete old files cannot look complete.
