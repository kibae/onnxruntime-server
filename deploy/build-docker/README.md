# Docker Build

## x64 with CUDA

- [ONNX Runtime Binary](https://github.com/microsoft/onnxruntime/releases) v1.30.0(latest) supports CUDA 12/13, cuDNN 9.
- Two CUDA variants are available:
    - `linux-cuda13`: Built on `nvidia/cuda:13.0.3-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:13.0.3-cudnn-runtime-ubuntu24.04`
    - `linux-cuda12`: Built on `nvidia/cuda:12.8.1-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:12.8.1-cudnn-runtime-ubuntu24.04`
- Multi-stage build is used to keep the final image small (devel for building, runtime for the final image).
- **We need to check this for each release of the ONNX Runtime binary.**

## Release image preparation and publication

`VERSION` records the server image version and `ORT_VERSION` pins the upstream
runtime used by both Docker and CI. Downloads never select `latest`.

Run `bash deploy/build-docker/build.sh --prepare` from a clean committed tree.
It tests every selected amd64 image before pushing candidates, then writes an
ignored `out/release-<version>-<sha>/receipt.json` with their digests and source
tree. CPU candidates include arm64, but this command does not test arm64.

After merging the validated PR, compare the merge commit tree with the receipt
and run `bash deploy/build-docker/build.sh --publish=<receipt.json>` to promote
those digests without rebuilding. Existing release tags with another digest are
rejected. Registry updates across three tags are not atomic; a partial promotion
can be retried with the same receipt and cannot replace a different release.

`--target=cpu` and `--target=cuda` still select a subset during preparation.

Run `python3 -B test/release/test_release_workflow.py` from the repository root
for offline workflow regression checks. These tests replace external commands;
they do not build images, access registries or publish a release.
