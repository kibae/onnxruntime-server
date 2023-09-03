//
// Created by Kibae Shin on 2023/08/31.
//

#include <utility>

#include "../onnx_runtime_server.hpp"

Orts::task::session_task::session_task(onnx::session_manager *onnx_session_manager, const std::string &buf)
	: task(), onnx_session_manager(onnx_session_manager) {
	raw = json::parse(buf);
	if (!raw.is_object() || !raw.contains("model") || !raw.contains("version") || !raw["model"].is_string() ||
		!raw["version"].is_number_integer()) {
		throw std::runtime_error("Invalid session task");
	}

	model_name = raw["model"];
	model_version = raw["version"];
	if (raw.contains("data"))
		data = raw["data"];
	else
		data = json::object();

	assert(model_name.length() > 0);
	assert(model_version >= 0);
}

Orts::task::session_task::session_task(
	onnx::session_manager *onnx_session_manager, std::string model_name, int model_version, json data
)
	: task(), onnx_session_manager(onnx_session_manager), model_name(std::move(model_name)),
	  model_version(model_version), data(std::move(data)) {
}

Orts::task::session_task::session_task(
	onnx::session_manager *onnx_session_manager, const std::string &model_name, int model_version
)
	: session_task(onnx_session_manager, model_name, model_version, json::object()) {
}

std::shared_ptr<Orts::task::task>
Orts::task::create(onnx::session_manager *onnx_session_manager, int16_t type, const std::string &buf) {
	switch (type) {
	case Orts::task::CREATE_SESSION:
		return std::make_shared<Orts::task::create_session>(onnx_session_manager, buf);
	case Orts::task::EXECUTE_SESSION:
		return std::make_shared<Orts::task::execute_session>(onnx_session_manager, buf);
	case Orts::task::DESTROY_SESSION:
		return std::make_shared<Orts::task::destroy_session>(onnx_session_manager, buf);
	case Orts::task::LIST_SESSION:
		return std::make_shared<Orts::task::list_session>(onnx_session_manager);
	default:
		throw std::runtime_error("Invalid task type");
	}
}
