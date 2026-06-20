# Sample datatypes model

A synthetic model with no inputs whose outputs cover every tensor element data type that
onnxruntime can carry through a model (one `Constant` output per type). Used to verify the full
output-decoding path end to end in `src/test/unit/unit_test_context.cpp` (per-type decoding is also
unit-tested in `src/test/unit/unit_test_value_info.cpp`). Referenced by the server as model `sample`,
version `datatypes`.

Excluded: complex64/complex128 — onnxruntime cannot allocate complex tensors.

Tripwire: uint2 is included even though onnxruntime 1.27 mis-loads its packed initializer; the
end-to-end test asserts the broken value so a future onnxruntime fix flips the test red and
prompts us to flip the expectation back to the correct values.

- test/sample-onnx-generator/sample-datatypes.py
- http://server.11math.com/static/onnxruntime-server/sample/datatypes.onnx
