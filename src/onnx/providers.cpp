//
// Created by Kibae Shin on 25. 8. 14..
//

#include "../onnxruntime_server.hpp"

onnxruntime_server::onnx::providers::providers() {
	_providers = Ort::GetAvailableProviders();
	_has_cuda = std::find(_providers.begin(), _providers.end(), "CUDAExecutionProvider") != _providers.end();
}

const onnxruntime_server::onnx::providers onnxruntime_server::onnx::providers::available_providers;
