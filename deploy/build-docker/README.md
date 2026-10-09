# Docker Build

## x64 with CUDA

- [ONNX Runtime Binary](https://github.com/microsoft/onnxruntime/releases) v1.31.0(latest) supports CUDA 12/13, cuDNN 9.
- Two CUDA variants are available:
    - `linux-cuda13`: Built on `nvidia/cuda:13.0.3-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:13.0.3-cudnn-runtime-ubuntu24.04`
    - `linux-cuda12`: Built on `nvidia/cuda:12.8.1-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:12.8.1-cudnn-runtime-ubuntu24.04`
- Multi-stage build is used to keep the final image small (devel for building, runtime for the final image).
- **We need to check this for each release of the ONNX Runtime binary.**

## Build, test and publish

`VERSION` records the server image version and `ORT_VERSION` pins the upstream
runtime used by both Docker and CI. Downloads never select `latest`.

The tags retain the existing format: `kibaes/onnxruntime-server:<version>-linux-cpu`,
`<version>-linux-cuda12`, and `<version>-linux-cuda13`. The script prints these tags
before building, and prints each command before executing it.

```bash
# Inspect the release tags and commands without building or pushing.
bash deploy/build-docker/build.sh --dry-run

# Build and test locally, including uncommitted changes. No uploads.
bash deploy/build-docker/build.sh --local

# After committing the verified changes and passing CI, build, test and push.
bash deploy/build-docker/build.sh
```

`--target=cpu` and `--target=cuda` select a subset in either mode.
`--local --dry-run` previews only the local commands. Local validation runs all
selected amd64 image tests before any upload. CPU publication builds amd64+arm64;
arm64 is not execution-tested by this script. Normal publication uses the build
cache. With no mode flag, the script builds, tests and pushes the selected images.

Run `python3 -B test/release/test_release_workflow.py` for offline regression checks
of tag selection, command ordering, local-only behavior and failure handling. These
replace external commands and do not establish Docker compatibility; validate changed
Docker behavior with the real `--local` command before committing.
