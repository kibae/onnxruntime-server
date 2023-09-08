//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnxruntime_server.hpp"

std::string onnxruntime_server::task::get_session::name() {
	return "GET_SESSION";
}

Orts::task::get_session::get_session(onnx::session_manager *onnx_session_manager, const std::string &buf)
	: session_task(onnx_session_manager, buf) {
}

Orts::task::get_session::get_session(
	Orts::onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version
)
	: session_task(onnx_session_manager, std::move(model_name), std::move(model_version)) {
}

json Orts::task::get_session::run() {
	auto session = onnx_session_manager->get_session(model_name, model_version);
	if (session == nullptr) {
		throw not_found_error("session not found");
	}

	return session->to_json();
}
