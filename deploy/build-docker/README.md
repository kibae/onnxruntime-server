# Docker Build

## x64 with CUDA

- [ONNX Runtime Binary](https://github.com/microsoft/onnxruntime/releases) v1.24.3(latest) supports CUDA 12/13, cuDNN 9.
- Two CUDA variants are available:
    - `linux-cuda13`: Built on `nvidia/cuda:13.1.1-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:13.1.1-cudnn-runtime-ubuntu24.04`
    - `linux-cuda12`: Built on `nvidia/cuda:12.9.1-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:12.9.1-cudnn-runtime-ubuntu24.04`
- Multi-stage build is used to keep the final image small (devel for building, runtime for the final image).
- **We need to check this for each release of the ONNX Runtime binary.**
