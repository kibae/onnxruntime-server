//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnxruntime_server.hpp"

Orts::task::destroy_session::destroy_session(onnx::session_manager *onnx_session_manager, const std::string &buf)
	: session_task(onnx_session_manager, buf) {
}

Orts::task::destroy_session::destroy_session(
	Orts::onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version
)
	: session_task(onnx_session_manager, std::move(model_name), std::move(model_version)) {
}

json Orts::task::destroy_session::run() {
	onnx_session_manager->remove_session(model_name, model_version);

	return json::boolean_t{true};
}
