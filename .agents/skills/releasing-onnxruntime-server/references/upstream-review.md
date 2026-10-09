# Upstream review and CUDA alignment

Read all `notes-*.md` in `(ORT_BASELINE, TO_VERSION]` for breaking changes, features,
deprecations, ONNX/opset and archive naming. Inspect `api.diff` for signatures and
enum-value changes as well as the extracted symbol/setter/header deltas.

| Change | Server mirror | Review |
| --- | --- | --- |
| Tensor element type | `src/onnx/value_info.cpp`, `src/onnx/execution/input_value.cpp` | Type name, output decoder, input behavior, tests |
| Graph optimization/execution enum | `src/onnx/session.cpp`, OpenAPI | String mapping, schema enum and echoed-options round trip |
| SessionOptions setter | `apply_session_options`, OpenAPI SessionOptions | JSON shape, forward to ORT, echo only successful setters |
| Config key | `config_entries` forwards arbitrary keys | Refresh relevant examples |
| CUDA provider key | `src/onnx/cuda/session_options.cpp` forwards keys | Documentation and compatibility |
| ONNX/opset | `test/sample-onnx-generator/*.py` | Appropriate fixtures/generator dependencies |
| Release assets | Installer, Docker/CI downloaders | Exact version/name and usable archive layout |

New types need names/decoders and `unit_test_value_info` coverage even if ORT cannot
construct them (complex64/128 precedent). If constructible, update `sample-datatypes.py`,
regenerate its tracked model and extend `AllDataTypesModel`. Check that CI fixture
cache restoration does not overwrite changed tracked fixtures.

Search src/test for version claims (`onnxruntime 1.x`, `ORT 1.x`). Preserve original
comments when still true and record revalidation in the PR. The uint2 `EXPECT_NE`
in `unit_test_context.cpp` is a known-loader-bug tripwire: if fixed, assert correct
values. Do not turn arbitrary failures into skipped tests or weaker inequalities.

Historical failure locations:

- Windows string tensor access across ORT DLL: use `GetStringTensorElement`, not
  `GetTensorData<std::string>()`.
- Windows hosted runner without GPU: `ONNX_RUNTIME_WITH_CUDA_PROVIDER=OFF`.
- Asset/API failure: inspect names/auth and fail on bad extraction. Do not silently
  fall back to an unpinned Homebrew runtime.

Read CUDA packaging at the immutable tag and follow referenced build commits/images.
SONAME provides a major ABI, not exact toolkit minor. Preserve Ubuntu base and CUDA
major symlink paths unless the actual layout requires otherwise. TensorRT packaging
does not alone justify changing a provider this server does not use.

Inspect builder and runtime metadata. Keep FULL `NVIDIA_REQUIRE_*`: spaces mean OR,
commas AND, multiple variables AND. Compare library compatibility, container admission
and measured host behavior separately. Lower admission constraints do not prove
bundled libraries run on every older driver. Historical WSL errors are host observations,
not universal statements about `NVIDIA_DISABLE_REQUIRE` or all GPUs/drivers.

Check current primary sources for published driver claims:

- https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/docker-specialized.html#constraints
- https://docs.nvidia.com/deploy/cuda-compatibility/minor-version-compatibility.html
- https://docs.nvidia.com/cuda/cuda-toolkit-release-notes/

Record upstream CUDA/cuDNN versions, exact sources, chosen image tags, full before/after
constraints and measured GPU results in the base-alignment commit and PR.
