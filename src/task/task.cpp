//
// Created by Kibae Shin on 2023/08/31.
//

#include <utility>

#include "../onnxruntime_server.hpp"

Orts::task::session_task::session_task(onnx::session_manager *onnx_session_manager, const json &request_json)
	: task(), onnx_session_manager(onnx_session_manager) {
	if (!request_json.is_object() || !request_json.contains("model") || !request_json.contains("version") ||
		!request_json["model"].is_string() || !request_json["version"].is_string()) {
		throw bad_request_error(
			"Invalid session task. Must be a JSON object with model(string) and version(string) fields"
		);
	}

	model_name = request_json["model"];
	model_version = request_json["version"];

	assert(model_name.length() > 0);
	assert(model_version.length() > 0);
}

Orts::task::session_task::session_task(
	onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version
)
	: task(), onnx_session_manager(onnx_session_manager), model_name(std::move(model_name)),
	  model_version(std::move(model_version)) {
}
