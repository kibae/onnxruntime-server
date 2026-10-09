# Sample datatypes model

A synthetic model with no inputs whose outputs cover every tensor element data type that
onnxruntime can carry through a model (one `Constant` output per type). Used to verify the full
output-decoding path end to end in `src/test/unit/unit_test_context.cpp` (per-type decoding is also
unit-tested in `src/test/unit/unit_test_value_info.cpp`). Referenced by the server as model `sample`,
version `datatypes`.

Excluded: complex64/complex128 — onnxruntime cannot allocate complex tensors.

UINT2 is packed four values per byte. ONNX Runtime 1.31 corrects the C API type conversion;
the end-to-end test verifies the decoded values `[3, 0, 2, 1, 3]`.

- test/sample-onnx-generator/sample-datatypes.py
- http://server.11math.com/static/onnxruntime-server/sample/datatypes.onnx
