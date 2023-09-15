//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnxruntime_server.hpp"

std::string onnxruntime_server::task::execute_session::name() {
	return "EXECUTE_SESSION";
}

Orts::task::execute_session::execute_session(onnx::session_manager *onnx_session_manager, const json &request_json)
	: session_task(onnx_session_manager, request_json) {
	if (!request_json.is_object() || !request_json.contains("data") || !request_json["data"].is_object()) {
		throw bad_request_error("Invalid session task. Must be a JSON object with data(object) field");
	}
	data = request_json["data"];
}

Orts::task::execute_session::execute_session(
	onnx::session_manager *onnx_session_manager, const std::string &model_name, const std::string &model_version,
	json data
)
	: session_task(onnx_session_manager, model_name, model_version), data(std::move(data)) {
}

json Orts::task::execute_session::run() {
	auto session = onnx_session_manager->get_session(model_name, model_version);
	if (session == nullptr) {
		throw not_found_error("session not found");
	}
	session->touch();

	Orts::onnx::execution::context ctx(session, data);
	auto result = ctx.run();
	return ctx.tensors_to_json(result);
}
