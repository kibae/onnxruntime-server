# Docker Build

## x64 with CUDA

- [ONNX Runtime Binary](https://github.com/microsoft/onnxruntime/releases) v1.20.2(latest) requires CUDA 11/12, cudnn 8/9.
  ```
  $ ldd libonnxruntime_providers_cuda.so 
  linux-vdso.so.1 (0x00007fffa4bf8000)
  libcublasLt.so.11 => /lib/x86_64-linux-gnu/libcublasLt.so.11 (0x00007f2ef0e00000)
  libcublas.so.11 => /lib/x86_64-linux-gnu/libcublas.so.11 (0x00007f2ee7200000)
  libcudnn.so.8 => /lib/x86_64-linux-gnu/libcudnn.so.8 (0x00007f2ee6e00000)
  libcurand.so.10 => /lib/x86_64-linux-gnu/libcurand.so.10 (0x00007f2ee1200000)
  libcufft.so.10 => /lib/x86_64-linux-gnu/libcufft.so.10 (0x00007f2ed8600000)
  libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007f2f1afbe000)
  librt.so.1 => /lib/x86_64-linux-gnu/librt.so.1 (0x00007f2f1afb9000)
  libcudart.so.11.0 => /lib/x86_64-linux-gnu/libcudart.so.11.0 (0x00007f2ed8200000)
  libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f2ed7e00000)
  libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f2f1aed2000)
  libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f2f1aeb2000)
  libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f2ed7a00000)
  /lib64/ld-linux-x86-64.so.2 (0x00007f2f1afde000)
  libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007f2f1aeab000)
  ```
- The docker image built from `nvidia/cuda:11.8.0-cudnn8-devel-ubuntu22.04` is very large, so we extract the binaries
  after build and create a docker image based on `nvidia/cuda:11.8.0-cudnn8-runtime-ubuntu22.04`.
- **We need to check this for each release of the ONNX Runtime binary.**
