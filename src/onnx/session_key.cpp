//
// Created by Kibae Shin on 2023/09/01.
//

#include <regex>
#include <utility>

#include "../onnxruntime_server.hpp"

Orts::onnx::session_key::session_key(std::string model_name, std::string model_version)
	: model_name(std::move(model_name)), model_version(std::move(model_version)) {
	// check model_name is_valid_model_name
	if (!is_valid_model_name(this->model_name)) {
		throw runtime_error("Invalid model name: " + this->model_name + " Must be \"a-z, A-Z, 0-9, '-', '_'.\"");
	}

	// check model_version is_valid_model_version
	if (!is_valid_model_version(this->model_version)) {
		throw runtime_error(
			"Invalid model version: " + this->model_version + " Must be \"a-z, A-Z, 0-9, '-', '_', '/'.\""
		);
	}
}

bool Orts::onnx::session_key::operator<(const Orts::onnx::session_key &other) const {
	if (model_name < other.model_name)
		return true;
	if (model_name > other.model_name)
		return false;
	return model_version < other.model_version;
}

bool onnxruntime_server::onnx::session_key::is_valid_model_name(const std::string &model_key) {
	if (model_key.empty())
		return false;
	// model_key must alnum, -, -
	return std::all_of(model_key.begin(), model_key.end(), [](char c) {
		return std::isalnum(c) || c == '_' || c == '-';
	});
}

bool onnxruntime_server::onnx::session_key::is_valid_model_version(const std::string &model_version) {
	if (model_version.empty())
		return false;
	// model_version must alnum, -, -, /
	return std::all_of(model_version.begin(), model_version.end(), [](char c) {
		return std::isalnum(c) || c == '_' || c == '-' || c == '/';
	});
}
