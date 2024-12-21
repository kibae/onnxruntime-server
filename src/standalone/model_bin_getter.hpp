//
// Created by kibae on 24. 12. 21.
//

#ifndef ONNXRUNTIME_SERVER_MODEL_BIN_GETTER_HPP
#define ONNXRUNTIME_SERVER_MODEL_BIN_GETTER_HPP

#include "../onnxruntime_server.hpp"
#include <fstream>
#include <sstream>

namespace onnxruntime_server {
	std::string get_model_bin(const boost::filesystem::path &model_root, const std::string &model_name, const std::string &model_version) {
		auto model_path = (model_root / model_name / model_version / "model.onnx").string();
		//check model path exists
		if (!std::filesystem::exists(model_path)) {
			auto path = (model_root / model_name / model_version);
			path += ".onnx";
			model_path = path.string();
			if (!std::filesystem::exists(model_path)) {
				throw std::runtime_error("Model file not found: " + model_path);
			}
		}

		std::ifstream file(model_path, std::ios::binary);
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
}

#endif // ONNXRUNTIME_SERVER_MODEL_BIN_GETTER_HPP
