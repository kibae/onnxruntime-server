//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnxruntime_server.hpp"

Orts::task::create_session::create_session(onnx::session_manager *onnx_session_manager, const std::string &buf)
	: session_task(onnx_session_manager, buf) {
	option = raw["option"];
}

Orts::task::create_session::create_session(
	Orts::onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version, json data,
	json option
)
	: session_task(onnx_session_manager, std::move(model_name), std::move(model_version), std::move(data)),
	  option(std::move(option)) {
}

json Orts::task::create_session::run() {
	Orts::onnx::session *session = nullptr;
	if (data.is_binary()) {
		auto binary_data = data.get_binary();
		std::string bin(binary_data.begin(), binary_data.end());
		// TODO: check convert
		session = onnx_session_manager->create_session(model_name, model_version, bin, option);
	} else {
		session = onnx_session_manager->create_session(model_name, model_version, option);
	}

	if (session == nullptr) {
		throw std::runtime_error("session not found");
	}

	return session->to_json();
}
