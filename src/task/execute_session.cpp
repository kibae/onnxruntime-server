//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnxruntime_server.hpp"

std::string onnxruntime_server::task::execute_session::name() {
	return "EXECUTE_SESSION";
}

Orts::task::execute_session::execute_session(onnx::session_manager *onnx_session_manager, const std::string &buf)
	: session_task(onnx_session_manager, buf) {
}

Orts::task::execute_session::execute_session(
	Orts::onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version, json data
)
	: session_task(onnx_session_manager, std::move(model_name), std::move(model_version), std::move(data)) {
}

json Orts::task::execute_session::run() {
	auto session = onnx_session_manager->get_session(model_name, model_version);
	if (session == nullptr) {
		throw std::runtime_error("session not found");
	}
	session->touch();

	Orts::onnx::execution::context ctx(session, data);
	auto result = ctx.run();
	return ctx.tensors_to_json(result);
}
