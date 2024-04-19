//
// Created by kibae on 24. 4. 20.
//

#include "../onnxruntime_server.hpp"

std::string onnxruntime_server::onnx::version() {
	return Ort::GetVersionString();
}
