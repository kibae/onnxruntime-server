//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnxruntime_server.hpp"

Orts::onnx::session_key::session_key(std::string model_name, std::string model_version)
	: model_name(std::move(model_name)), model_version(std::move(model_version)) {
}

bool Orts::onnx::session_key::operator<(const Orts::onnx::session_key &other) const {
	if (model_name < other.model_name)
		return true;
	if (model_name > other.model_name)
		return false;
	return model_version < other.model_version;
}
