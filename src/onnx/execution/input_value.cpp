//
// Created by Kibae Shin on 2023/09/02.
//
#include "../../onnxruntime_server.hpp"

Orts::onnx::execution::input_value::~input_value() {
	for (auto &p : deallocators) {
		p();
	}
}

#define SHAPE_ARG batched_shape(info.shape, values->size()).data(), info.shape.size()
#define ORT_VALUE_RETURN(t)                                                                                            \
	{                                                                                                                  \
		auto *values = new std::vector<t>();                                                                           \
		for (auto &val : json_value)                                                                                   \
			values->emplace_back((t)val.get<t>());                                                                     \
                                                                                                                       \
		tensors = Ort::Value::CreateTensor<t>(memory_info, values->data(), values->size(), SHAPE_ARG);                 \
		deallocators.emplace_back([values]() { delete values; });                                                      \
		break;                                                                                                         \
	}

Orts::onnx::execution::input_value::input_value(
	const Ort::MemoryInfo &memory_info, const value_info &info, const json::value_type &json_value
) {
	switch (info.element_type) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: // maps to c type float
		ORT_VALUE_RETURN(float_t);
		/*
		{
			// for debugging
			auto *values = new std::vector<float_t>();
			for (auto &val : json_value)
				values->emplace_back((float_t)val.get<float_t>());

			tensors = Ort::Value::CreateTensor<float_t>(memory_info, values->data(), values->size(), SHAPE_ARG);
			deallocators.emplace_back([values]() { delete values; });
			break;
		}
		*/
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: // maps to c type double
		ORT_VALUE_RETURN(double_t);

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: // maps to c type int8_t
		ORT_VALUE_RETURN(int8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: // maps to c type int16_t
		ORT_VALUE_RETURN(int16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: // maps to c type int32_t
		ORT_VALUE_RETURN(int32_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: // maps to c type int64_t
		ORT_VALUE_RETURN(int64_t);

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: // maps to c type uint8_t
		ORT_VALUE_RETURN(uint8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: // maps to c type uint16_t
		ORT_VALUE_RETURN(uint16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: // maps to c type uint32_t
		ORT_VALUE_RETURN(uint32_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: // maps to c type uint64_t
		ORT_VALUE_RETURN(uint64_t);

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: //
	{
		// Because std::vector<bool> stores in bits to save memory, it doesn't have a .data() method,
		// so we use std::vector<char>.
		auto values = new bool[json_value.size()];
		for (size_t i = 0; i < json_value.size(); i++)
			values[i] = json_value[i].get<bool>();

		tensors = Ort::Value::CreateTensor<bool>(
			memory_info, values, json_value.size(), batched_shape(info.shape, json_value.size()).data(),
			info.shape.size()
		);
		deallocators.emplace_back([values]() { delete[] values; });
		break;
	}

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: // Non-IEEE floating-point format based on IEEE754 single-precision
	{
		auto *values = new std::vector<Ort::BFloat16_t>();
		for (auto &val : json_value)
			values->emplace_back(val.get<float>());

		tensors = Ort::Value::CreateTensor<Ort::BFloat16_t>(memory_info, values->data(), values->size(), SHAPE_ARG);
		deallocators.emplace_back([values]() { delete values; });
		break;
	}

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: //
	{
		auto *values = new std::vector<Ort::Float16_t>();
		for (auto &val : json_value)
			values->emplace_back(val.get<float>());

		tensors = Ort::Value::CreateTensor<Ort::Float16_t>(memory_info, values->data(), values->size(), SHAPE_ARG);
		deallocators.emplace_back([values]() { delete values; });
		break;
	}

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: // maps to c++ type std::string
	{
		auto *values = new std::vector<std::string>();
		for (auto &val : json_value)
			values->emplace_back(val.get<std::string>());

		// cannot use Ort::Value::CreateTensor<> generic
		tensors = Ort::Value::CreateTensor(
			memory_info, values->data(), values->size() * sizeof(std::string), SHAPE_ARG,
			ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING
		);
		deallocators.emplace_back([values]() { delete values; });
		break;
	}

	default:
		throw bad_request_error("Not supported type: " + info.type_name());
	}
}

#undef ORT_VALUE_RETURN
#undef SHAPE_ARG

std::vector<int64_t>
onnxruntime_server::onnx::execution::input_value::batched_shape(const std::vector<int64_t> &shape, size_t value_count) {
	// check shape contains -1
	if (std::find(shape.begin(), shape.end(), -1) == shape.end())
		return shape;

	// calculate batch size
	std::vector<int64_t> shape_copy = shape;
	int64_t batch = (int64_t)value_count;
	for (auto &s : shape_copy) {
		if (s != -1)
			batch = (int64_t)std::max((double)1, std::ceil((double)batch / (double)s));
	}

	// replace -1 with batch size
	for (auto &s : shape_copy) {
		if (s == -1) {
			s = batch;
			break;
		}
	}

	return shape_copy;
}
