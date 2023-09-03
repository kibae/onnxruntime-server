//
// Created by Kibae Shin on 2023/09/02.
//

#include "../../onnx_runtime_server.hpp"

Orts::onnx::execution::context::context(Orts::onnx::session *session, const json &json_str)
	: memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)), session(session) {
	assert(session != nullptr);

	json dataset = json_str;

	if (!dataset.is_object() && dataset.is_string())
		dataset = json::parse(dataset.get<std::string>());

	// check dataset is object
	if (!dataset.is_object()) {
		throw std::runtime_error("Top-level JSON dataset is not object");
	}

	for (auto &input : session->inputs()) {
		if (!dataset[input.name].is_array())
			throw std::runtime_error("Input " + input.name + " is not array");

		// batch first. first dimension is batch size
		std::vector<json::value_type> json_values;
		flat_json_values(dataset[input.name], &json_values);

		inputs[input.name] = new Orts::onnx::execution::input_value(memory_info, input, json_values);
	}
}

Orts::onnx::execution::context::~context() {
	for (auto &p : inputs) {
		delete p.second;
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

	for (auto &input : inputs) {
		input_values.emplace_back(std::move(input.second->tensors));
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
