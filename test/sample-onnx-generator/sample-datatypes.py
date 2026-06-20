"""
Generate an ONNX model whose outputs cover every tensor element data type that
onnxruntime-server can decode (see src/onnx/value_info.cpp).

Each output is a `Constant` node holding known values, so the model takes no inputs and can be run
to exercise the server's output decoding for all data types at once. The exotic sub-byte / float8
types are emitted as raw bytes using the exact encodings asserted in
src/test/unit/unit_test_value_info.cpp, so the model carries values whose decoded form is known.

Excluded: complex64 / complex128 — onnxruntime cannot allocate complex tensors, so a model
containing them fails to load. value_info still names them and has a defensive decoder.

Tripwire: uint2 is included even though onnxruntime 1.27 mis-loads packed uint2 initializers (the
runtime values come out as [3, 3, 0, 0, 0] instead of the embedded [3, 0, 2, 1, 3]; int2 with the
same structure works). AllDataTypesModel asserts the broken value so the test will start failing as
soon as upstream fixes the bug — that is the signal to flip the expectation back to the embedded
values.

Usage (from this directory):
    python3 -m venv venv && . venv/bin/activate
    pip install onnx onnxruntime numpy
    python sample-datatypes.py

Writes ../fixture/sample/datatypes/model.onnx (referenced by the server as model "sample",
version "datatypes").
"""
import os

import numpy as np
import onnx
import onnxruntime as ort
from onnx import TensorProto, helper

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.normpath(os.path.join(HERE, "..", "fixture", "sample", "datatypes", "model.onnx"))

# name -> (TensorProto type, dims, TensorProto value).
# Standard types use value lists; reduced-precision / sub-byte types use the raw byte encodings from
# the unit tests. The trailing comment shows the values the server is expected to decode.
TYPES = [
    ("float32", TensorProto.FLOAT, [3], helper.make_tensor("float32", TensorProto.FLOAT, [3], [1.5, -2.25, 0.0])),
    ("float64", TensorProto.DOUBLE, [2], helper.make_tensor("float64", TensorProto.DOUBLE, [2], [3.5, -7.0])),
    ("int8", TensorProto.INT8, [3], helper.make_tensor("int8", TensorProto.INT8, [3], [-128, 0, 127])),
    ("int16", TensorProto.INT16, [2], helper.make_tensor("int16", TensorProto.INT16, [2], [-32768, 32767])),
    ("int32", TensorProto.INT32, [2], helper.make_tensor("int32", TensorProto.INT32, [2], [-2147483648, 2147483647])),
    ("int64", TensorProto.INT64, [1], helper.make_tensor("int64", TensorProto.INT64, [1], [-9223372036854775807])),
    ("uint8", TensorProto.UINT8, [2], helper.make_tensor("uint8", TensorProto.UINT8, [2], [0, 255])),
    ("uint16", TensorProto.UINT16, [1], helper.make_tensor("uint16", TensorProto.UINT16, [1], [65535])),
    ("uint32", TensorProto.UINT32, [1], helper.make_tensor("uint32", TensorProto.UINT32, [1], [4294967295])),
    ("uint64", TensorProto.UINT64, [1], helper.make_tensor("uint64", TensorProto.UINT64, [1], [18446744073709551615])),
    ("bool", TensorProto.BOOL, [3], helper.make_tensor("bool", TensorProto.BOOL, [3], [1, 0, 1])),
    ("string", TensorProto.STRING, [2], helper.make_tensor("string", TensorProto.STRING, [2], [b"hello", b"world"])),
    ("float16", TensorProto.FLOAT16, [3], onnx.numpy_helper.from_array(np.array([1.5, -2.0, 0.5], dtype=np.float16), "float16")),
    # bfloat16: 1.5, -2.0, 4.0 (little-endian 16-bit patterns 0x3FC0, 0xC000, 0x4080)
    ("bfloat16", TensorProto.BFLOAT16, [3], helper.make_tensor("bfloat16", TensorProto.BFLOAT16, [3], vals=bytes([0xC0, 0x3F, 0x00, 0xC0, 0x80, 0x40]), raw=True)),
    # float8e5m2: 1, 2, -1, 0, +inf, NaN
    ("float8e5m2", TensorProto.FLOAT8E5M2, [6], helper.make_tensor("float8e5m2", TensorProto.FLOAT8E5M2, [6], vals=bytes([0x3C, 0x40, 0xBC, 0x00, 0x7C, 0x7D]), raw=True)),
    # float8e5m2fnuz: 1, 0, NaN
    ("float8e5m2fnuz", TensorProto.FLOAT8E5M2FNUZ, [3], helper.make_tensor("float8e5m2fnuz", TensorProto.FLOAT8E5M2FNUZ, [3], vals=bytes([0x40, 0x00, 0x80]), raw=True)),
    # float8e4m3fn: 1, 2, -1, 448 (max), NaN
    ("float8e4m3fn", TensorProto.FLOAT8E4M3FN, [5], helper.make_tensor("float8e4m3fn", TensorProto.FLOAT8E4M3FN, [5], vals=bytes([0x38, 0x40, 0xB8, 0x7E, 0x7F]), raw=True)),
    # float8e4m3fnuz: 1, 0, NaN
    ("float8e4m3fnuz", TensorProto.FLOAT8E4M3FNUZ, [3], helper.make_tensor("float8e4m3fnuz", TensorProto.FLOAT8E4M3FNUZ, [3], vals=bytes([0x40, 0x00, 0x80]), raw=True)),
    # float8e8m0: 2^0=1, 2^1=2, 2^-1=0.5, NaN
    ("float8e8m0", TensorProto.FLOAT8E8M0, [4], helper.make_tensor("float8e8m0", TensorProto.FLOAT8E8M0, [4], vals=bytes([127, 128, 126, 0xFF]), raw=True)),
    # float4e2m1: 0.0, 1.5, 6.0, -2.0 (packed two-per-byte: 0x30, 0xC7)
    ("float4e2m1", TensorProto.FLOAT4E2M1, [4], helper.make_tensor("float4e2m1", TensorProto.FLOAT4E2M1, [4], vals=bytes([0x30, 0xC7]), raw=True)),
    # int4: -8, 7, 1 (packed: 0x78, 0x01)
    ("int4", TensorProto.INT4, [3], helper.make_tensor("int4", TensorProto.INT4, [3], vals=bytes([0x78, 0x01]), raw=True)),
    # uint4: 15, 0, 9 (packed: 0x0F, 0x09)
    ("uint4", TensorProto.UINT4, [3], helper.make_tensor("uint4", TensorProto.UINT4, [3], vals=bytes([0x0F, 0x09]), raw=True)),
    # int2: -2, -1, 0, 1 (packed four-per-byte: 0x4E)
    ("int2", TensorProto.INT2, [4], helper.make_tensor("int2", TensorProto.INT2, [4], vals=bytes([0x4E]), raw=True)),
    # uint2: nominal value [3, 0, 2, 1, 3], packed as 0x63, 0x03. onnxruntime 1.27 mis-loads packed
    # uint2 initializers (it treats uint2 as unpacked storage and memcpy's the packed bytes into a
    # 5-byte buffer, producing the runtime values [3, 3, 0, 0, 0]). int2 with the same structure
    # works. AllDataTypesModel asserts the broken value so that when upstream fixes the bug the
    # test fails and prompts us to flip the expectation to [3, 0, 2, 1, 3]. See the ORT issue
    # referenced in the test for details.
    ("uint2", TensorProto.UINT2, [5], helper.make_tensor("uint2", TensorProto.UINT2, [5], vals=bytes([0x63, 0x03]), raw=True)),
]


def main():
    nodes, outputs = [], []
    for name, tp, dims, value in TYPES:
        nodes.append(helper.make_node("Constant", [], [name], value=value))
        outputs.append(helper.make_tensor_value_info(name, tp, dims))

    graph = helper.make_graph(nodes, "all_data_types", [], outputs)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 23)])
    model.ir_version = 10
    model.doc_string = "onnxruntime-server test fixture: one output per supported tensor data type"

    # Validate the graph and that onnxruntime can load it (this is what the C++ server does).
    onnx.checker.check_model(model)
    ort.InferenceSession(model.SerializeToString(), providers=["CPUExecutionProvider"])

    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    onnx.save(model, OUT_PATH)
    print(f"wrote {OUT_PATH}")
    print(f"outputs ({len(TYPES)}): {', '.join(name for name, *_ in TYPES)}")
    print("excluded: complex64, complex128 (onnxruntime cannot allocate complex tensors)")


if __name__ == "__main__":
    main()
