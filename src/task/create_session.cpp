//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnxruntime_server.hpp"

#include <boost/beast/core/detail/base64.hpp>

namespace detail = boost::beast::detail;

std::string onnxruntime_server::task::create_session::name() {
	return "CREATE_SESSION";
}

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
	std::string model_bin;
	if (!data.is_null() && !data.empty() && data.is_object() && data.contains("bytes")) {
		if (data["enc"] == "base64") {
			auto bytes = data["bytes"].get<std::string>();
			model_bin.resize(detail::base64::decoded_size(bytes.size()));
			auto decoded_size = detail::base64::decode(model_bin.data(), bytes.data(), bytes.size());
			model_bin.resize(decoded_size.first);
		} else {
			auto binary_data = data["bytes"].get_binary();
			model_bin.append(binary_data.begin(), binary_data.end());
		}
	}

	if (!model_bin.empty()) {
		session = onnx_session_manager->create_session(model_name, model_version, model_bin, option);
	} else {
		session = onnx_session_manager->create_session(model_name, model_version, option);
	}

	if (session == nullptr) {
		throw not_found_error("session not found");
	}

	return session->to_json();
}
