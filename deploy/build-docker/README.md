# Docker Build

## x64 with CUDA

- [ONNX Runtime Binary](https://github.com/microsoft/onnxruntime/releases) v1.30.0(latest) supports CUDA 12/13, cuDNN 9.
- Two CUDA variants are available:
    - `linux-cuda13`: Built on `nvidia/cuda:13.0.3-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:13.0.3-cudnn-runtime-ubuntu24.04`
    - `linux-cuda12`: Built on `nvidia/cuda:12.8.1-cudnn-devel-ubuntu24.04`, runtime based on `nvidia/cuda:12.8.1-cudnn-runtime-ubuntu24.04`
- Multi-stage build is used to keep the final image small (devel for building, runtime for the final image).
- **We need to check this for each release of the ONNX Runtime binary.**
