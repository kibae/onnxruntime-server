//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnxruntime_server.hpp"

std::string onnxruntime_server::task::destroy_session::name() {
	return "DESTROY_SESSION";
}

Orts::task::destroy_session::destroy_session(onnx::session_manager *onnx_session_manager, const json &request_json)
	: session_task(onnx_session_manager, request_json) {
}

Orts::task::destroy_session::destroy_session(
	onnx::session_manager *onnx_session_manager, const std::string &model_name, const std::string &model_version
)
	: session_task(onnx_session_manager, model_name, model_version) {
}

json Orts::task::destroy_session::run() {
	onnx_session_manager->remove_session(model_name, model_version);

	return json::boolean_t{true};
}
