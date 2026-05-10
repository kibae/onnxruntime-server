//
// Created by kibae on 23. 9. 5.
//
#include "session_options.hpp"

#include <set>

namespace {

std::string to_provider_string(const json &v) {
	if (v.is_boolean())
		return v.get<bool>() ? "1" : "0";
	if (v.is_number_integer())
		return std::to_string(v.get<int64_t>());
	if (v.is_number_unsigned())
		return std::to_string(v.get<uint64_t>());
	if (v.is_string())
		return v.get<std::string>();
	return v.dump();
}

// Apply all caller-supplied CUDA provider options in a single UpdateCUDAProviderOptions call.
//
// Why a single call: ORT V2's UpdateCUDAProviderOptions silently resets sibling keys that share
// an internal options group (e.g. updating arena_extend_strategy alone reverts gpu_mem_limit to
// its default). Calling it once with the full key/value set is the only way to apply multiple
// keys safely. The trade-off is that any single invalid key aborts the whole batch; that is
// acceptable here because ORT's error message identifies the offending key, so the caller can
// see exactly what was rejected.
void update_all(OrtCUDAProviderOptionsV2 *cuda_options, const std::vector<std::string> &keys,
				const std::vector<std::string> &values) {
	if (keys.empty())
		return;
	std::vector<const char *> ck;
	std::vector<const char *> cv;
	ck.reserve(keys.size());
	cv.reserve(values.size());
	for (size_t i = 0; i < keys.size(); ++i) {
		ck.push_back(keys[i].c_str());
		cv.push_back(values[i].c_str());
	}
	OrtStatus *st = Ort::GetApi().UpdateCUDAProviderOptions(cuda_options, ck.data(), cv.data(), ck.size());
	if (st != nullptr) {
		const char *err = Ort::GetApi().GetErrorMessage(st);
		std::string msg = err ? err : "unknown error";
		Ort::GetApi().ReleaseStatus(st);
		throw onnxruntime_server::runtime_error(std::string("Failed to update CUDA provider options: ") + msg);
	}
}

// Convert the readback value (always a string from ORT) back to the most natural JSON type so
// the response shape matches what callers typically send: integers as integers, "true"/"false"
// as booleans, anything else as a string.
json infer_readback_value(const std::string &raw) {
	if (raw == "true")
		return true;
	if (raw == "false")
		return false;
	if (!raw.empty()) {
		bool numeric = (raw[0] == '-' || (raw[0] >= '0' && raw[0] <= '9'));
		if (numeric) {
			for (size_t i = 1; i < raw.size(); ++i) {
				if (raw[i] < '0' || raw[i] > '9') {
					numeric = false;
					break;
				}
			}
			if (numeric) {
				try {
					return json(std::stoll(raw));
				} catch (...) {
					// fall through to string
				}
			}
		}
	}
	return raw;
}

// Parse "key1=value1;key2=value2" produced by GetCUDAProviderOptionsAsString.
json parse_options_string(const std::string &s) {
	json out = json::object();
	size_t pos = 0;
	while (pos < s.size()) {
		auto eq = s.find('=', pos);
		if (eq == std::string::npos)
			break;
		auto sc = s.find(';', eq);
		if (sc == std::string::npos)
			sc = s.size();
		auto k = s.substr(pos, eq - pos);
		auto v = s.substr(eq + 1, sc - eq - 1);
		if (!k.empty())
			out[k] = infer_readback_value(v);
		pos = sc + 1;
	}
	return out;
}

} // namespace

// Apply CUDA provider options.
//
// Validation policy mirrors apply_session_options(): we forward every shape-valid input entry to
// ORT one key at a time; ORT decides whether to accept it. The echoed object is built from
// GetCUDAProviderOptionsAsString readback (the ground truth of what ORT stored), filtered to the
// keys the caller actually supplied (plus device_id, which is always meaningful).
json append_cuda_session_options(OrtSessionOptions *session_options, const json &option) {
	auto cuda = option["cuda"];

	OrtCUDAProviderOptionsV2 *cuda_options = nullptr;
	Ort::ThrowOnError(Ort::GetApi().CreateCUDAProviderOptions(&cuda_options));

	// Track which keys the caller asked about — these are the keys we will echo from readback.
	// device_id is always included for backward compatibility with the previous response shape.
	std::set<std::string> requested_keys;
	requested_keys.insert("device_id");

	std::vector<std::string> keys;
	std::vector<std::string> values;
	if (cuda.is_object()) {
		for (auto it = cuda.begin(); it != cuda.end(); ++it) {
			keys.push_back(it.key());
			values.push_back(to_provider_string(it.value()));
			requested_keys.insert(it.key());
		}
	} else if (cuda.is_number_integer()) {
		keys.push_back("device_id");
		values.push_back(std::to_string(cuda.get<int>()));
	}
	// cuda == true or false: nothing to update; default V2 options are used.

	try {
		update_all(cuda_options, keys, values);
	} catch (...) {
		Ort::GetApi().ReleaseCUDAProviderOptions(cuda_options);
		throw;
	}

	OrtStatus *append_status =
		Ort::GetApi().SessionOptionsAppendExecutionProvider_CUDA_V2(session_options, cuda_options);
	if (append_status != nullptr) {
		const char *err = Ort::GetApi().GetErrorMessage(append_status);
		std::string msg = err ? err : "unknown error";
		Ort::GetApi().ReleaseStatus(append_status);
		Ort::GetApi().ReleaseCUDAProviderOptions(cuda_options);
		throw onnxruntime_server::runtime_error(std::string("Failed to append CUDA EP: ") + msg);
	}

	// Readback the full options string and echo only the keys the caller cared about.
	// The whole readback section is wrapped in a try/catch so that an exception in any of the
	// allocations (std::bad_alloc, json construction) cannot leak the ORT allocator buffer or the
	// cuda_options handle.
	json result = json::object();
	try {
		OrtAllocator *allocator = nullptr;
		OrtStatus *alloc_st = Ort::GetApi().GetAllocatorWithDefaultOptions(&allocator);
		if (alloc_st != nullptr) {
			Ort::GetApi().ReleaseStatus(alloc_st);
		} else {
			char *cstr = nullptr;
			OrtStatus *st = Ort::GetApi().GetCUDAProviderOptionsAsString(cuda_options, allocator, &cstr);
			if (st != nullptr) {
				Ort::GetApi().ReleaseStatus(st);
			} else if (cstr != nullptr) {
				try {
					auto all = parse_options_string(std::string(cstr));
					for (auto it = all.begin(); it != all.end(); ++it) {
						if (requested_keys.count(it.key()))
							result[it.key()] = it.value();
					}
				} catch (...) {
					allocator->Free(allocator, cstr);
					throw;
				}
				allocator->Free(allocator, cstr);
			}
		}
	} catch (...) {
		Ort::GetApi().ReleaseCUDAProviderOptions(cuda_options);
		throw;
	}

	Ort::GetApi().ReleaseCUDAProviderOptions(cuda_options);

	return result;
}
