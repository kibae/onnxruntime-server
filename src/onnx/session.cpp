//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnx_runtime_server.hpp"

Orts::onnx::session::session(session_key key)
	: key(std::move(key)), created_at(std::chrono::system_clock::now()), allocator() {
}

Orts::onnx::session::session(session_key key, const std::string &path, const Ort::SessionOptions &session_options)
	: session(std::move(key)) {

	ort_session = new Ort::Session(env, path.c_str(), session_options);

	init();
}

Orts::onnx::session::session(
	session_key key, const void *model_data, size_t modal_data_length, const Ort::SessionOptions &session_options
)
	: session(std::move(key)) {
	ort_session = new Ort::Session(env, model_data, modal_data_length, session_options);

	init();
}

void Orts::onnx::session::init() {
	// input metadata
	inputCount = ort_session->GetInputCount();
	for (size_t i = 0; i < inputCount; i++) {
		auto name = ort_session->GetInputNameAllocated(i, allocator);
		_inputNames.emplace_back(name.get());

		auto info = ort_session->GetInputTypeInfo(i);
		auto tensorType = info.GetTensorTypeAndShapeInfo();
		auto shape = tensorType.GetShape();
		_inputs.emplace_back(name.get(), tensorType.GetElementType(), shape);
	}
	for (auto &name : _inputNames)
		inputNames.push_back(name.c_str());

	// output metadata
	outputCount = ort_session->GetOutputCount();
	for (size_t i = 0; i < outputCount; i++) {
		auto name = ort_session->GetOutputNameAllocated(i, allocator);
		_outputNames.emplace_back(name.get());

		auto info = ort_session->GetOutputTypeInfo(i);
		auto tensorType = info.GetTensorTypeAndShapeInfo();
		auto shape = tensorType.GetShape();
		_outputs.emplace_back(name.get(), tensorType.GetElementType(), shape);
	}
	for (auto &name : _outputNames)
		outputNames.push_back(name.c_str());
}

Orts::onnx::session::~session() {
	delete ort_session;
}

void Orts::onnx::session::touch() {
	last_executed_at = std::chrono::system_clock::now();
	execution_count++;
}

const std::vector<Orts::onnx::value_info> &Orts::onnx::session::inputs() const {
	return _inputs;
}

const std::vector<Orts::onnx::value_info> &Orts::onnx::session::outputs() const {
	return _outputs;
}

std::vector<Ort::Value>
Orts::onnx::session::run(const Ort::MemoryInfo &memory_info, const std::vector<Ort::Value> &input_values) {
	if (input_values.empty() || input_values.size() != inputCount) {
		throw std::runtime_error("params size is not same as: " + std::to_string(inputCount));
	}

	Ort::RunOptions options;

	return ort_session->Run(
		options, inputNames.data(), input_values.data(), inputCount, outputNames.data(), outputCount
	);
}

json onnx_runtime_server::onnx::session::to_json() const {
	json::object_t dict;
	dict["model"] = key.model_name;
	dict["version"] = key.model_version;
	dict["created_at"] = std::chrono::system_clock::to_time_t(created_at);
	dict["last_executed_at"] = std::chrono::system_clock::to_time_t(last_executed_at);
	dict["execution_count"] = execution_count;

	json::object_t inputs;
	for (auto &input : _inputs) {
		inputs[input.name] = input.type_to_string();
	}
	dict["inputs"] = inputs;

	json::object_t outputs;
	for (auto &output : _outputs) {
		outputs[output.name] = output.type_to_string();
	}
	dict["outputs"] = outputs;

	return dict;
}
