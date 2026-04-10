//
// Created by Kibae Shin on 2023/09/02.
//

#include "../../onnxruntime_server.hpp"

Orts::onnx::execution::context::context(std::shared_ptr<Orts::onnx::session> session, const json &json_str)
	: memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)), session(session) {
	assert(session != nullptr);

	json dataset = json_str;

	if (!dataset.is_object() && dataset.is_string())
		dataset = json::parse(dataset.get<std::string>());

	// check dataset is object
	if (!dataset.is_object()) {
		throw bad_request_error("Top-level JSON dataset is not object");
	}

	for (auto input : session->inputs()) {
		if (!dataset[input.name].is_array())
			throw bad_request_error("Input " + input.name + " is not array");

		// batch first. first dimension is batch size
		std::vector<json::value_type> json_values;
		flat_json_values(dataset[input.name], &json_values);

		std::vector<int64_t> input_shape;
		calcShape(dataset[input.name], input.shape.size(), &input_shape);
		input.set_input_shape(input_shape);

		inputs[input.name] = new Orts::onnx::execution::input_value(memory_info, input, json_values);
	}
}

Orts::onnx::execution::context::~context() {
	for (auto &p : inputs) {
		delete p.second;
	}
}

void Orts::onnx::execution::context::calcShape(
	const json::value_type &data, int count, std::vector<int64_t> *input_shape
) {
	bool is_first_val = true;
	if (data.is_array() && count != 0) {
		for (auto &item : data) {
			if (is_first_val) {
				is_first_val = false;
				input_shape->push_back(data.size());
				calcShape(item, count - 1, input_shape);
			} else
				break;
		}
	}

	if (input_shape->size() == count - 1) {
		input_shape->push_back(1);
	}
}

void Orts::onnx::execution::context::flat_json_values(
	const json::value_type &data, std::vector<json::value_type> *json_values
) {
	if (data.is_array()) {
		for (auto &item : data) {
			flat_json_values(item, json_values);
		}
	} else {
		json_values->push_back(data);
	}
}

std::vector<Ort::Value> Orts::onnx::execution::context::run() {
	std::vector<Ort::Value> input_values;
	input_values.reserve(inputs.size());

	// Fix: iterate in session-defined order, not map alphabetical order
	for (auto &input_info : session->inputs()) {
		input_values.emplace_back(std::move(inputs[input_info.name]->tensors));
	}

	return this->session->run(memory_info, input_values);
}

json Orts::onnx::execution::context::tensors_to_json(std::vector<Ort::Value> &tensors) {
	auto infos = session->outputs();
	json::object_t output;
	for (int i = 0; i < tensors.size(); i++) {
		auto &info = infos[i];
		auto &item = tensors[i];

		output[info.name] = info.get_tensor_data(item);
	}

	return output;
}
