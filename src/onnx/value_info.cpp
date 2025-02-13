//
// Created by Kibae Shin on 2023/09/02.
//

#include "../onnxruntime_server.hpp"

Orts::onnx::value_info::value_info(std::string name, ONNXTensorElementDataType element_type, std::vector<int64_t> shape)
	: name(std::move(name)), element_type(element_type), shape(std::move(shape)) {
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
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
		return "float16";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: // maps to c type double
		return "float64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: // maps to c type uint32_t
		return "uint32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: // maps to c type uint64_t
		return "uint64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64: // complex with float32 real and imaginary components
		return "complex64(Not Supported)";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128: // complex with float64 real and imaginary components
		return "complex128(Not Supported)";
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

json::array_t Orts::onnx::value_info::get_tensor_data(Ort::Value &tensors) const {
	json::array_t values;
	auto size = tensors.GetTensorTypeAndShapeInfo().GetElementCount();

	switch (element_type) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
		break;
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: // maps to c type float
		GET_TENSOR_DATA(float);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: // maps to c type uint8_t
		GET_TENSOR_DATA(uint8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: // maps to c type int8_t
		GET_TENSOR_DATA(int8_t);
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
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2: //
		GET_TENSOR_FLOAT_DATA(Ort::Float8E5M2_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ: //
		GET_TENSOR_FLOAT_DATA(Ort::Float8E5M2FNUZ_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN: //
		GET_TENSOR_FLOAT_DATA(Ort::Float8E4M3FN_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ: //
		GET_TENSOR_FLOAT_DATA(Ort::Float8E4M3FNUZ_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: //
		GET_TENSOR_FLOAT_DATA(Ort::Float16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: // Non-IEEE floating-point format based on IEEE754 single-precision
		GET_TENSOR_FLOAT_DATA(Ort::BFloat16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64: // complex with float32 real and imaginary components
		break;
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128: // complex with float64 real and imaginary components
		break;
	}

	auto dims = tensors.GetTensorTypeAndShapeInfo().GetShape();
	return values_fit_shape(values, dims, dims.size());
}

#undef GET_TENSOR_DATA
#undef GET_TENSOR_FLOAT_DATA
