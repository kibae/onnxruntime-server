//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnxruntime_server.hpp"

#ifdef HAS_CUDA
#include "cuda/session_options.hpp"
#endif

Orts::onnx::session::session(session_key key, const json &option)
	: session_options(), created_at(std::chrono::system_clock::now()), allocator(), key(std::move(key)) {
	_option["cuda"] = false;

	if (option.contains("cuda") && (!option["cuda"].is_boolean() || option["cuda"].get<bool>())) {
#ifdef HAS_CUDA
		_option["cuda"] = append_cuda_session_options(session_options, option);
#else
		throw runtime_error("CUDA is not supported");
#endif
	}

	if (option.contains("input_shape") && option["input_shape"].is_object())
		_option["input_shape"] = option["input_shape"];
	if (option.contains("output_shape") && option["output_shape"].is_object())
		_option["output_shape"] = option["output_shape"];
}

Orts::onnx::session::session(session_key key, const std::string &path, const json &option)
	: session(std::move(key), option) {
#ifdef _WIN32
	int size_needed = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &wstr[0], size_needed);

	auto model_path = wstr.c_str();
#else
	auto model_path = path.c_str();
#endif

	ort_session = new Ort::Session(env, model_path, session_options);
	init();
}

Orts::onnx::session::session(
	session_key key, const char *model_data, const size_t model_data_length, const json &option
)
	: session(std::move(key), option) {
	ort_session = new Ort::Session(env, model_data, model_data_length, session_options);
	init();
}

void Orts::onnx::session::init() {
	assert(ort_session != nullptr);

	// input metadata
	inputCount = ort_session->GetInputCount();
	for (size_t i = 0; i < inputCount; i++) {
		auto name = ort_session->GetInputNameAllocated(i, allocator);
		_inputNames.emplace_back(name.get());

		auto info = ort_session->GetInputTypeInfo(i);
		auto tensorType = info.GetTensorTypeAndShapeInfo();
		auto shape = tensorType.GetShape();
		_inputs.emplace_back(name.get(), tensorType.GetElementType(), shape);

		override_shape("input_shape", _inputs.back());
	}
	_option.erase("input_shape");
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

		override_shape("output_shape", _outputs.back());
	}
	_option.erase("output_shape");
	for (auto &name : _outputNames)
		outputNames.push_back(name.c_str());

	PLOG(L_DEBUG) << "Session created: " << key.model_name << "/" << key.model_version << std::endl;
}

void onnxruntime_server::onnx::session::override_shape(const char *option_key, value_info &value) {
	if (_option.contains(option_key) && _option[option_key].contains(value.name)) {
		auto override_shape = _option[option_key][value.name];
		if (override_shape.is_array()) {
			if (override_shape.size() != value.shape.size())
				throw runtime_error(std::string(option_key) + " override size mismatch: " + override_shape.dump());

			for (auto p = 0; p < value.shape.size(); p++) {
				if (p < override_shape.size())
					value.shape[p] = override_shape[p].get<int64_t>();
			}
		}
	}
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
	assert(ort_session != nullptr);

	if (input_values.empty() || input_values.size() != inputCount) {
		throw runtime_error("params size is not same as: " + std::to_string(inputCount));
	}

	Ort::RunOptions options;

	return ort_session->Run(
		options, inputNames.data(), input_values.data(), inputCount, outputNames.data(), outputCount
	);
}

json onnxruntime_server::onnx::session::to_json() const {
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
	dict["option"] = _option;

	return dict;
}
