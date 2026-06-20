//
// Created by Kibae Shin on 2026/06/20.
//
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "../../onnxruntime_server.hpp"

using Orts::onnx::value_info;

namespace {
// Build a tensor from raw bytes with an explicit element type, then run it through
// value_info::get_tensor_data exactly as the server would for a model output.
json::array_t decode(ONNXTensorElementDataType type, const std::vector<int64_t> &shape, const void *bytes, size_t byte_count) {
	auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	auto tensor = Ort::Value::CreateTensor(
		mem, const_cast<void *>(bytes), byte_count, shape.data(), shape.size(), type
	);
	value_info vi("t", type, shape);
	return vi.get_tensor_data(tensor);
}

template <typename T>
json::array_t decode_typed(ONNXTensorElementDataType type, const std::vector<int64_t> &shape, const std::vector<T> &vals) {
	return decode(type, shape, vals.data(), vals.size() * sizeof(T));
}
} // namespace

// ---------------------------------------------------------------------------------------------------
// type_name: every element type onnxruntime 1.27 exposes must map to its documented string. Anything
// not listed here would fall through to "unknown", so this is the full contract.
// ---------------------------------------------------------------------------------------------------
TEST(unit_test_value_info, TypeNames) {
	struct {
		ONNXTensorElementDataType type;
		const char *name;
	} cases[] = {
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED, "undefined"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, "float32"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE, "float64"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8, "int8"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16, "int16"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32, "int32"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, "int64"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8, "uint8"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16, "uint16"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32, "uint32"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64, "uint64"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4, "int4"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4, "uint4"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_INT2, "int2"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT2, "uint2"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, "boolean"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING, "string"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, "float16"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16, "bfloat16"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2, "float8e5m2"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ, "float8e5m2fnuz"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN, "float8e4m3fn"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ, "float8e4m3fnuz"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E8M0, "float8e8m0"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT4E2M1, "float4e2m1"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64, "complex64"},
		{ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128, "complex128"},
	};
	for (const auto &c : cases)
		EXPECT_STREQ(value_info::type_name(c.type), c.name) << "type enum " << c.type;
}

// ---------------------------------------------------------------------------------------------------
// get_tensor_data: full-width primitive types map straight through to JSON numbers.
// ---------------------------------------------------------------------------------------------------
TEST(unit_test_value_info, Float32) {
	auto v = decode_typed<float>(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {3}, {1.5f, -2.25f, 0.0f});
	ASSERT_EQ(v.size(), 3);
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 1.5);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), -2.25);
	EXPECT_DOUBLE_EQ(v[2].get<double>(), 0.0);
}

TEST(unit_test_value_info, Float64) {
	auto v = decode_typed<double>(ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE, {2}, {3.5, -7.0});
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 3.5);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), -7.0);
}

TEST(unit_test_value_info, SignedIntegers) {
	auto i8 = decode_typed<int8_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8, {3}, {-128, 0, 127});
	EXPECT_EQ(i8[0].get<int>(), -128);
	EXPECT_EQ(i8[2].get<int>(), 127);

	auto i16 = decode_typed<int16_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16, {2}, {-32768, 32767});
	EXPECT_EQ(i16[0].get<int>(), -32768);
	EXPECT_EQ(i16[1].get<int>(), 32767);

	auto i32 = decode_typed<int32_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32, {2}, {-2147483647 - 1, 2147483647});
	EXPECT_EQ(i32[0].get<int32_t>(), -2147483648);
	EXPECT_EQ(i32[1].get<int32_t>(), 2147483647);

	auto i64 = decode_typed<int64_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, {1}, {-9223372036854775807LL});
	EXPECT_EQ(i64[0].get<int64_t>(), -9223372036854775807LL);
}

TEST(unit_test_value_info, UnsignedIntegers) {
	auto u8 = decode_typed<uint8_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8, {2}, {0, 255});
	EXPECT_EQ(u8[0].get<unsigned>(), 0u);
	EXPECT_EQ(u8[1].get<unsigned>(), 255u);

	auto u16 = decode_typed<uint16_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16, {1}, {65535});
	EXPECT_EQ(u16[0].get<unsigned>(), 65535u);

	auto u32 = decode_typed<uint32_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32, {1}, {4294967295u});
	EXPECT_EQ(u32[0].get<uint32_t>(), 4294967295u);

	auto u64 = decode_typed<uint64_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64, {1}, {18446744073709551615ULL});
	EXPECT_EQ(u64[0].get<uint64_t>(), 18446744073709551615ULL);
}

TEST(unit_test_value_info, Bool) {
	uint8_t bytes[] = {1, 0, 1};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, {3}, bytes, sizeof(bytes));
	ASSERT_EQ(v.size(), 3);
	EXPECT_TRUE(v[0].get<bool>());
	EXPECT_FALSE(v[1].get<bool>());
	EXPECT_TRUE(v[2].get<bool>());
}

TEST(unit_test_value_info, String) {
	Ort::AllocatorWithDefaultOptions alloc;
	std::vector<int64_t> shape{2};
	auto tensor = Ort::Value::CreateTensor(alloc, shape.data(), shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING);
	const char *strs[] = {"hello", "world"};
	tensor.FillStringTensor(strs, 2);

	value_info vi("s", ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING, shape);
	auto v = vi.get_tensor_data(tensor);
	ASSERT_EQ(v.size(), 2);
	EXPECT_EQ(v[0].get<std::string>(), "hello");
	EXPECT_EQ(v[1].get<std::string>(), "world");
}

// ---------------------------------------------------------------------------------------------------
// get_tensor_data: reduced-precision floats convert to their numeric value.
// ---------------------------------------------------------------------------------------------------
TEST(unit_test_value_info, Float16) {
	std::vector<Ort::Float16_t> h = {Ort::Float16_t(1.5f), Ort::Float16_t(-2.0f), Ort::Float16_t(0.5f)};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, {3}, h.data(), h.size() * sizeof(Ort::Float16_t));
	EXPECT_FLOAT_EQ(v[0].get<float>(), 1.5f);
	EXPECT_FLOAT_EQ(v[1].get<float>(), -2.0f);
	EXPECT_FLOAT_EQ(v[2].get<float>(), 0.5f);
}

TEST(unit_test_value_info, BFloat16) {
	std::vector<Ort::BFloat16_t> h = {Ort::BFloat16_t(1.5f), Ort::BFloat16_t(-2.0f), Ort::BFloat16_t(4.0f)};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16, {3}, h.data(), h.size() * sizeof(Ort::BFloat16_t));
	EXPECT_FLOAT_EQ(v[0].get<float>(), 1.5f);
	EXPECT_FLOAT_EQ(v[1].get<float>(), -2.0f);
	EXPECT_FLOAT_EQ(v[2].get<float>(), 4.0f);
}

// The Ort::Float8*_t wrappers only convert to uint8_t, so these decode the IEEE-style fields by hand.
// Bytes below are canonical float8 encodings for each format.
TEST(unit_test_value_info, Float8E5M2) {
	uint8_t bytes[] = {0x3C, 0x40, 0xBC, 0x00, 0x7C, 0x7D}; // 1, 2, -1, 0, +inf, NaN
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2, {6}, bytes, sizeof(bytes));
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), 2.0);
	EXPECT_DOUBLE_EQ(v[2].get<double>(), -1.0);
	EXPECT_DOUBLE_EQ(v[3].get<double>(), 0.0);
	EXPECT_TRUE(std::isinf(v[4].get<double>()));
	EXPECT_TRUE(std::isnan(v[5].get<double>()));
}

TEST(unit_test_value_info, Float8E4M3FN) {
	uint8_t bytes[] = {0x38, 0x40, 0xB8, 0x7E, 0x7F}; // 1, 2, -1, 448 (max), NaN
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN, {5}, bytes, sizeof(bytes));
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), 2.0);
	EXPECT_DOUBLE_EQ(v[2].get<double>(), -1.0);
	EXPECT_DOUBLE_EQ(v[3].get<double>(), 448.0);
	EXPECT_TRUE(std::isnan(v[4].get<double>()));
}

TEST(unit_test_value_info, Float8E4M3FNUZ) {
	uint8_t bytes[] = {0x40, 0x00, 0x80}; // 1, 0, NaN
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ, {3}, bytes, sizeof(bytes));
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), 0.0);
	EXPECT_TRUE(std::isnan(v[2].get<double>()));
}

TEST(unit_test_value_info, Float8E5M2FNUZ) {
	uint8_t bytes[] = {0x40, 0x00, 0x80}; // 1, 0, NaN
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ, {3}, bytes, sizeof(bytes));
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), 0.0);
	EXPECT_TRUE(std::isnan(v[2].get<double>()));
}

// FLOAT8E8M0: unsigned exponent-only format (bias 127); each value is a power of two and 0xFF is NaN.
TEST(unit_test_value_info, Float8E8M0) {
	uint8_t bytes[] = {127, 128, 126, 0xFF}; // 2^0=1, 2^1=2, 2^-1=0.5, NaN
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E8M0, {4}, bytes, sizeof(bytes));
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), 2.0);
	EXPECT_DOUBLE_EQ(v[2].get<double>(), 0.5);
	EXPECT_TRUE(std::isnan(v[3].get<double>()));
}

// ---------------------------------------------------------------------------------------------------
// get_tensor_data: packed sub-byte types must be unpacked into one JSON value per logical element.
// ---------------------------------------------------------------------------------------------------

// FLOAT4E2M1 magnitudes {0,0.5,1,1.5,2,3,4,6} with bit 3 as sign, packed two-per-byte.
// {0.0, 1.5, 6.0, -2.0} -> nibbles 0x0,0x3 (=0x30), 0x7,0xC (=0xC7).
TEST(unit_test_value_info, Float4E2M1) {
	uint8_t bytes[] = {0x30, 0xC7};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT4E2M1, {4}, bytes, sizeof(bytes));
	ASSERT_EQ(v.size(), 4);
	EXPECT_DOUBLE_EQ(v[0].get<double>(), 0.0);
	EXPECT_DOUBLE_EQ(v[1].get<double>(), 1.5);
	EXPECT_DOUBLE_EQ(v[2].get<double>(), 6.0);
	EXPECT_DOUBLE_EQ(v[3].get<double>(), -2.0);
}

// UINT4 is packed two-per-byte, low nibble first. {15, 0, 9} -> bytes 0x0F, 0x09.
TEST(unit_test_value_info, UInt4) {
	uint8_t bytes[] = {0x0F, 0x09};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4, {3}, bytes, sizeof(bytes));
	ASSERT_EQ(v.size(), 3);
	EXPECT_EQ(v[0].get<int>(), 15);
	EXPECT_EQ(v[1].get<int>(), 0);
	EXPECT_EQ(v[2].get<int>(), 9);
}

// INT4 is two's complement, packed two-per-byte. {-8, 7, 1} -> nibbles 0x8,0x7 (=0x78), 0x1.
TEST(unit_test_value_info, Int4) {
	uint8_t bytes[] = {0x78, 0x01};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4, {3}, bytes, sizeof(bytes));
	ASSERT_EQ(v.size(), 3);
	EXPECT_EQ(v[0].get<int>(), -8);
	EXPECT_EQ(v[1].get<int>(), 7);
	EXPECT_EQ(v[2].get<int>(), 1);
}

// Unlike int2, onnxruntime 1.27 stores uint2 unpacked: one value (0..3) per byte.
TEST(unit_test_value_info, UInt2) {
	uint8_t bytes[] = {3, 0, 2, 1, 3};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT2, {5}, bytes, sizeof(bytes));
	ASSERT_EQ(v.size(), 5);
	int expected[] = {3, 0, 2, 1, 3};
	for (int i = 0; i < 5; i++)
		EXPECT_EQ(v[i].get<int>(), expected[i]);
}

// INT2 two's complement (-2..1), packed four-per-byte. {-2,-1,0,1} -> bits 2,3,0,1 -> byte 0x4E.
TEST(unit_test_value_info, Int2) {
	uint8_t bytes[] = {0x4E};
	auto v = decode(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT2, {4}, bytes, sizeof(bytes));
	ASSERT_EQ(v.size(), 4);
	int expected[] = {-2, -1, 0, 1};
	for (int i = 0; i < 4; i++)
		EXPECT_EQ(v[i].get<int>(), expected[i]);
}

// ---------------------------------------------------------------------------------------------------
// Shape handling and the complex limitation.
// ---------------------------------------------------------------------------------------------------

// values_fit_shape must nest the flat data according to the tensor's dimensions.
TEST(unit_test_value_info, MultiDimensionalShape) {
	auto v = decode_typed<float>(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {2, 2}, {1.0f, 2.0f, 3.0f, 4.0f});
	ASSERT_EQ(v.size(), 2);
	ASSERT_TRUE(v[0].is_array());
	EXPECT_DOUBLE_EQ(v[0][0].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(v[0][1].get<double>(), 2.0);
	EXPECT_DOUBLE_EQ(v[1][0].get<double>(), 3.0);
	EXPECT_DOUBLE_EQ(v[1][1].get<double>(), 4.0);
}

// onnxruntime 1.27 does not support allocating complex tensors, so get_tensor_data's complex
// branches cannot be exercised through a real Ort::Value. This test documents that limitation: if a
// future version starts supporting complex tensors, CreateTensor will stop throwing and this test
// will fail, signalling that the (already implemented) {real, imag} decode should get real coverage.
TEST(unit_test_value_info, ComplexNotConstructibleInOrt127) {
	auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	std::vector<int64_t> shape{2};
	float c64[] = {1.0f, 2.0f, 3.0f, -1.0f};
	double c128[] = {1.0, 2.0, 3.0, -1.0};
	EXPECT_ANY_THROW(
		Ort::Value::CreateTensor(
			mem, c64, sizeof(c64), shape.data(), shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64
		)
	);
	EXPECT_ANY_THROW(
		Ort::Value::CreateTensor(
			mem, c128, sizeof(c128), shape.data(), shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128
		)
	);
}
