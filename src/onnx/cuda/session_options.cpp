//
// Created by kibae on 23. 9. 5.
//
#include "../../onnx_runtime_server.hpp"

json append_cuda_session_options(OrtSessionOptions *session_options, const json &option) {
	auto cuda = option["cuda"];

	json result = json::object();

	// device_id
	int device_id = 0;
	if (cuda.is_object() && cuda.contains("device_id"))
		device_id = cuda["device_id"].get<int>();
	result["device_id"] = device_id;

	Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CUDA(session_options, device_id));

	return result;
}
