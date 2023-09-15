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

Orts::task::create_session::create_session(
	onnx::session_manager *onnx_session_manager, const json &request_json, const char *model_data,
	size_t model_data_length
)
	: session_task(onnx_session_manager, request_json), model_data(model_data), model_data_length(model_data_length) {
	option =
		request_json.contains("option") && request_json["option"].is_object() ? request_json["option"] : json::object();
}

Orts::task::create_session::create_session(
	onnx::session_manager *onnx_session_manager, const std::string &model_name, const std::string &model_version,
	json option, const char *model_data, size_t model_data_length
)
	: session_task(onnx_session_manager, model_name, model_version), option(std::move(option)), model_data(model_data),
	  model_data_length(model_data_length) {
}

json Orts::task::create_session::run() {
	Orts::onnx::session *session =
		onnx_session_manager->create_session(model_name, model_version, option, model_data, model_data_length);
	if (session == nullptr) {
		throw not_found_error("session not found");
	}

	return session->to_json();
}
