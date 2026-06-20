//
// Created by Kibae Shin on 2023/09/02.
//

#include "../onnxruntime_server.hpp"

#include <cmath>
#include <limits>

Orts::onnx::value_info::value_info(std::string name, ONNXTensorElementDataType element_type, std::vector<int64_t> shape)
	: name(std::move(name)), element_type(element_type), shape(std::move(shape)) {
}

void Orts::onnx::value_info::set_input_shape(std::vector<int64_t> input_shape) {
	this->input_shape = move(input_shape);
}

std::string Orts::onnx::value_info::type_name() const {
	return value_info::type_name(element_type);
}

std::string Orts::onnx::value_info::type_to_string() const {
	std::string str;
	for (auto &dim : shape) {
		if (!str.empty())
			str += ",";
		str += std::to_string(dim);
	}
	return type_name() + "[" + str + "]";
}

const char *Orts::onnx::value_info::type_name(const ONNXTensorElementDataType element_type) {
	switch (element_type) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
		return "undefined";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: // maps to c type float
		return "float32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4: //
		return "uint4";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4: //
		return "int4";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT2: //
		return "uint2";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT2: //
		return "int2";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: // maps to c type uint8_t
		return "uint8";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: // maps to c type int8_t
		return "int8";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: // maps to c type uint16_t
		return "uint16";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: // maps to c type int16_t
		return "int16";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: // maps to c type int32_t
		return "int32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: // maps to c type int64_t
		return "int64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: // maps to c++ type std::string
		return "string";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
		return "boolean";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2:
		return "float8e5m2";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ:
		return "float8e5m2fnuz";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN:
		return "float8e4m3fn";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ:
		return "float8e4m3fnuz";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E8M0:
		return "float8e8m0";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT4E2M1:
		return "float4e2m1";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
		return "float16";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: // maps to c type double
		return "float64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: // maps to c type uint32_t
		return "uint32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: // maps to c type uint64_t
		return "uint64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64: // complex with float32 real and imaginary components
		return "complex64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128: // complex with float64 real and imaginary components
		return "complex128";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: // Non-IEEE floating-point format based on IEEE754 single-precision
		return "bfloat16";
	default:
		return "unknown";
	}
}

json::array_t values_fit_shape(json::array_t &values, std::vector<int64_t> &shape, size_t depth) {
	depth--;
	if (depth <= 0)
		return values;

	auto dim = shape[depth];
	json::array_t result;

	for (size_t i = 0; i < values.size(); i += dim) {
		json::array_t row;
		for (size_t p = i; p < i + dim; p++) {
			row.emplace_back(values[p]);
		}
		result.emplace_back(row);
	}

	return values_fit_shape(result, shape, depth);
}

#define GET_TENSOR_DATA(TYPE)                                                                                          \
	{                                                                                                                  \
		auto data = tensors.GetTensorData<TYPE>();                                                                     \
		for (size_t i = 0; i < size; i++)                                                                              \
			values.emplace_back(data[i]);                                                                              \
		break;                                                                                                         \
	}

#define GET_TENSOR_FLOAT_DATA(TYPE)                                                                                    \
	{                                                                                                                  \
		auto data = tensors.GetTensorData<TYPE>();                                                                     \
		for (size_t i = 0; i < size; i++)                                                                              \
			values.emplace_back(static_cast<float>(data[i]));                                                          \
		break;                                                                                                         \
	}

#define GET_TENSOR_FLOAT8_DATA(EXP, MANT, BIAS, HAS_INF, FNUZ)                                                         \
	{                                                                                                                  \
		auto data = tensors.GetTensorData<uint8_t>();                                                                  \
		for (size_t i = 0; i < size; i++)                                                                              \
			values.emplace_back(decode_float8(data[i], EXP, MANT, BIAS, HAS_INF, FNUZ));                               \
		break;                                                                                                         \
	}

// Extract the i-th 4-bit value from packed storage (two nibbles per byte, low nibble holds the
// even-indexed element). Used by int4/uint4/float4e2m1, which onnxruntime stores packed.
static inline uint8_t unpack_nibble(const uint8_t *data, size_t i) {
	uint8_t byte = data[i / 2];
	return (i % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
}

// Extract the i-th 2-bit value from packed storage (four fields per byte, lowest field first).
// Used by int2/uint2.
static inline uint8_t unpack_2bit(const uint8_t *data, size_t i) {
	return (data[i / 4] >> ((i % 4) * 2)) & 0x03;
}

// Decode an 8-bit float (ONNX float8 family) to double. The Ort::Float8*_t wrappers only convert to
// uint8_t, so static_cast<float> on them yields the raw byte rather than the numeric value; we decode
// the IEEE-style fields by hand. exp_bits + mant_bits == 7. `has_inf` is true only for e5m2; `fnuz`
// is true for the *fnuz variants, whose only NaN is 0x80 and which have no infinities.
static double decode_float8(uint8_t byte, int exp_bits, int mant_bits, int bias, bool has_inf, bool fnuz) {
	const int exp = (byte >> mant_bits) & ((1 << exp_bits) - 1);
	const int mant = byte & ((1 << mant_bits) - 1);
	const int exp_max = (1 << exp_bits) - 1;
	const double sign = (byte & 0x80) ? -1.0 : 1.0;
	const double scale = static_cast<double>(1 << mant_bits);

	if (fnuz) {
		if (byte == 0x80)
			return std::numeric_limits<double>::quiet_NaN();
	} else if (exp == exp_max) {
		if (has_inf) // e5m2: mant 0 -> inf, otherwise NaN
			return mant == 0 ? sign * std::numeric_limits<double>::infinity()
							 : std::numeric_limits<double>::quiet_NaN();
		if (mant == (1 << mant_bits) - 1) // e4m3fn: only S.1111.111 is NaN
			return std::numeric_limits<double>::quiet_NaN();
		// e4m3fn other top-exponent values are ordinary finite numbers
	}
	if (exp == 0)
		return sign * std::ldexp(mant / scale, 1 - bias); // subnormal
	return sign * std::ldexp(1.0 + mant / scale, exp - bias); // normal
}

json::array_t Orts::onnx::value_info::get_tensor_data(Ort::Value &tensors) const {
	json::array_t values;
	auto size = tensors.GetTensorTypeAndShapeInfo().GetElementCount();

	switch (element_type) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
		break;
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: // maps to c type float
		GET_TENSOR_DATA(float);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: // maps to c type uint8_t
		GET_TENSOR_DATA(uint8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: // maps to c type int8_t
		GET_TENSOR_DATA(int8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4: { // packed: two uint4 (0..15) per byte
		auto data = tensors.GetTensorData<uint8_t>();
		for (size_t i = 0; i < size; i++)
			values.emplace_back(static_cast<unsigned>(unpack_nibble(data, i)));
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4: { // packed: two int4 (two's complement, -8..7) per byte
		auto data = tensors.GetTensorData<uint8_t>();
		for (size_t i = 0; i < size; i++) {
			uint8_t n = unpack_nibble(data, i);
			values.emplace_back(n >= 8 ? static_cast<int>(n) - 16 : static_cast<int>(n));
		}
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT2: { // onnxruntime 1.27 stores uint2 unpacked, one value (0..3) per byte
		auto data = tensors.GetTensorData<uint8_t>();
		for (size_t i = 0; i < size; i++)
			values.emplace_back(static_cast<unsigned>(data[i] & 0x03));
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT2: { // packed: four int2 (two's complement, -2..1) per byte
		auto data = tensors.GetTensorData<uint8_t>();
		for (size_t i = 0; i < size; i++) {
			uint8_t b = unpack_2bit(data, i);
			values.emplace_back(b >= 2 ? static_cast<int>(b) - 4 : static_cast<int>(b));
		}
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT4E2M1: { // packed: two e2m1 (1 sign, 2 exp, 1 mantissa) per byte
		// E2M1 has only 16 representable values; the 8 magnitudes form a fixed table and bit 3 is the sign.
		static const double e2m1_magnitude[8] = {0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0};
		auto data = tensors.GetTensorData<uint8_t>();
		for (size_t i = 0; i < size; i++) {
			uint8_t n = unpack_nibble(data, i);
			double v = e2m1_magnitude[n & 0x07];
			values.emplace_back((n & 0x08) ? -v : v);
		}
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: // maps to c type uint16_t
		GET_TENSOR_DATA(uint16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: // maps to c type int16_t
		GET_TENSOR_DATA(int16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: // maps to c type int32_t
		GET_TENSOR_DATA(int32_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: // maps to c type int64_t
		GET_TENSOR_DATA(int64_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: // maps to c++ type std::string
		GET_TENSOR_DATA(std::string);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: //
		GET_TENSOR_DATA(bool);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: // maps to c type double
		GET_TENSOR_DATA(double_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: // maps to c type uint32_t
		GET_TENSOR_DATA(uint32_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: // maps to c type uint64_t
		GET_TENSOR_DATA(uint64_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2: // 1 sign, 5 exponent, 2 mantissa, bias 15, has inf
		GET_TENSOR_FLOAT8_DATA(5, 2, 15, true, false);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ: // bias 16, no inf, NaN at 0x80
		GET_TENSOR_FLOAT8_DATA(5, 2, 16, false, true);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN: // 1 sign, 4 exponent, 3 mantissa, bias 7, no inf
		GET_TENSOR_FLOAT8_DATA(4, 3, 7, false, false);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ: // bias 8, no inf, NaN at 0x80
		GET_TENSOR_FLOAT8_DATA(4, 3, 8, false, true);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E8M0: { //
		// E8M0 has no Ort C++ wrapper type. It is an unsigned exponent-only format (8 exponent bits,
		// 0 mantissa, no sign, bias 127): every value is a power of two, and 0xFF encodes NaN.
		auto data = tensors.GetTensorData<uint8_t>();
		for (size_t i = 0; i < size; i++) {
			if (data[i] == 0xFF)
				values.emplace_back(std::numeric_limits<double>::quiet_NaN());
			else
				values.emplace_back(std::ldexp(1.0, static_cast<int>(data[i]) - 127));
		}
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: //
		GET_TENSOR_FLOAT_DATA(Ort::Float16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: // Non-IEEE floating-point format based on IEEE754 single-precision
		GET_TENSOR_FLOAT_DATA(Ort::BFloat16_t);
	// onnxruntime 1.27 cannot allocate complex tensors, so these branches are defensive: if a future
	// version starts emitting them, each element is interleaved (real, imag) and decodes to {real, imag}.
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64: { // interleaved float32 (real, imag) pairs
		auto data = tensors.GetTensorData<float>();
		for (size_t i = 0; i < size; i++) {
			json c;
			c["real"] = data[2 * i];
			c["imag"] = data[2 * i + 1];
			values.emplace_back(std::move(c));
		}
		break;
	}
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128: { // interleaved float64 (real, imag) pairs
		auto data = tensors.GetTensorData<double>();
		for (size_t i = 0; i < size; i++) {
			json c;
			c["real"] = data[2 * i];
			c["imag"] = data[2 * i + 1];
			values.emplace_back(std::move(c));
		}
		break;
	}
	}

	auto dims = tensors.GetTensorTypeAndShapeInfo().GetShape();
	return values_fit_shape(values, dims, dims.size());
}

#undef GET_TENSOR_DATA
#undef GET_TENSOR_FLOAT_DATA
#undef GET_TENSOR_FLOAT8_DATA
