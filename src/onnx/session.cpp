//
// Created by Kibae Shin on 2023/09/01.
//

#include <utility>

#include "../onnxruntime_server.hpp"

#ifdef HAS_CUDA
#include "cuda/session_options.hpp"
#endif

namespace {

#ifdef _WIN32
std::wstring to_wide(const std::string &s) {
	int size_needed = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &wstr[0], size_needed);
	return wstr;
}
#endif

void register_extension(Ort::SessionOptions &session_options, const std::string &path) {
#ifdef _WIN32
	auto wpath = to_wide(path);
	auto p = wpath.c_str();
#else
	auto p = path.c_str();
#endif
	OrtStatus *status = Ort::GetApi().RegisterCustomOpsLibrary_V2(session_options, p);
	if (status != nullptr) {
		const char *err = Ort::GetApi().GetErrorMessage(status);
		std::string msg = err ? err : "unknown error";
		Ort::GetApi().ReleaseStatus(status);
		throw onnxruntime_server::runtime_error(
			std::string("Failed to register ORT extensions (") + path + "): " + msg
		);
	}
}

GraphOptimizationLevel parse_graph_opt_level(const std::string &v, bool &valid) {
	valid = true;
	if (v == "disable" || v == "disabled" || v == "off")
		return ORT_DISABLE_ALL;
	if (v == "basic")
		return ORT_ENABLE_BASIC;
	if (v == "extended")
		return ORT_ENABLE_EXTENDED;
	if (v == "all")
		return ORT_ENABLE_ALL;
	valid = false;
	return ORT_ENABLE_ALL;
}

ExecutionMode parse_execution_mode(const std::string &v, bool &valid) {
	valid = true;
	if (v == "parallel")
		return ORT_PARALLEL;
	if (v == "sequential")
		return ORT_SEQUENTIAL;
	valid = false;
	return ORT_SEQUENTIAL;
}

// Apply session-level options.
//
// Validation policy: we only check JSON shape (types, our enum string mapping). We do NOT
// re-implement ORT's own value validation (allowed ranges, defaults, etc.) — that knowledge
// belongs to ORT and would force us to track every ORT version's rules. Instead, every shape-
// valid value is forwarded to ORT, and the setter's outcome decides the echo:
//   - setter succeeds  -> echo the value (or the readback value where an API exists)
//   - setter throws    -> skip silently, do not echo (the option was rejected by ORT)
// Where ORT exposes a readback (currently config_entries via GetSessionConfigEntry), the echo
// uses the readback value so it reflects what ORT actually stored, not what we sent.
template <typename F>
bool try_apply(F &&f) {
	try {
		f();
		return true;
	} catch (const Ort::Exception &) {
		return false;
	} catch (const std::exception &) {
		return false;
	}
}

json apply_session_options(Ort::SessionOptions &session_options, const json &input) {
	json applied = json::object();
	if (!input.is_object())
		return applied;

	if (input.contains("intra_op_num_threads") && input["intra_op_num_threads"].is_number_integer()) {
		auto v = input["intra_op_num_threads"].get<int>();
		if (try_apply([&] { session_options.SetIntraOpNumThreads(v); }))
			applied["intra_op_num_threads"] = v;
	}

	if (input.contains("inter_op_num_threads") && input["inter_op_num_threads"].is_number_integer()) {
		auto v = input["inter_op_num_threads"].get<int>();
		if (try_apply([&] { session_options.SetInterOpNumThreads(v); }))
			applied["inter_op_num_threads"] = v;
	}

	if (input.contains("execution_mode") && input["execution_mode"].is_string()) {
		bool valid = false;
		auto s = input["execution_mode"].get<std::string>();
		auto mode = parse_execution_mode(s, valid);
		if (valid && try_apply([&] { session_options.SetExecutionMode(mode); }))
			applied["execution_mode"] = (mode == ORT_PARALLEL) ? "parallel" : "sequential";
	}

	if (input.contains("graph_optimization_level") && input["graph_optimization_level"].is_string()) {
		bool valid = false;
		auto s = input["graph_optimization_level"].get<std::string>();
		auto lvl = parse_graph_opt_level(s, valid);
		if (valid && try_apply([&] { session_options.SetGraphOptimizationLevel(lvl); }))
			applied["graph_optimization_level"] = s;
	}

	if (input.contains("enable_cpu_mem_arena") && input["enable_cpu_mem_arena"].is_boolean()) {
		auto v = input["enable_cpu_mem_arena"].get<bool>();
		if (try_apply([&] {
				if (v)
					session_options.EnableCpuMemArena();
				else
					session_options.DisableCpuMemArena();
			}))
			applied["enable_cpu_mem_arena"] = v;
	}

	if (input.contains("enable_mem_pattern") && input["enable_mem_pattern"].is_boolean()) {
		auto v = input["enable_mem_pattern"].get<bool>();
		if (try_apply([&] {
				if (v)
					session_options.EnableMemPattern();
				else
					session_options.DisableMemPattern();
			}))
			applied["enable_mem_pattern"] = v;
	}

	if (input.contains("log_severity_level") && input["log_severity_level"].is_number_integer()) {
		auto v = input["log_severity_level"].get<int>();
		if (try_apply([&] { session_options.SetLogSeverityLevel(v); }))
			applied["log_severity_level"] = v;
	}

	if (input.contains("logid") && input["logid"].is_string()) {
		auto v = input["logid"].get<std::string>();
		if (try_apply([&] { session_options.SetLogId(v.c_str()); }))
			applied["logid"] = v;
	}

	if (input.contains("enable_profiling") && input["enable_profiling"].is_boolean() &&
		input["enable_profiling"].get<bool>()) {
		std::string prefix;
		if (input.contains("profile_file_prefix") && input["profile_file_prefix"].is_string())
			prefix = input["profile_file_prefix"].get<std::string>();
		bool ok = try_apply([&] {
#ifdef _WIN32
			auto wprefix = to_wide(prefix);
			session_options.EnableProfiling(wprefix.c_str());
#else
			session_options.EnableProfiling(prefix.c_str());
#endif
		});
		if (ok) {
			applied["enable_profiling"] = true;
			applied["profile_file_prefix"] = prefix;
		}
	}

	if (input.contains("optimized_model_filepath") && input["optimized_model_filepath"].is_string()) {
		auto s = input["optimized_model_filepath"].get<std::string>();
		bool ok = try_apply([&] {
#ifdef _WIN32
			auto ws = to_wide(s);
			session_options.SetOptimizedModelFilePath(ws.c_str());
#else
			session_options.SetOptimizedModelFilePath(s.c_str());
#endif
		});
		if (ok)
			applied["optimized_model_filepath"] = s;
	}

	if (input.contains("free_dimension_overrides") && input["free_dimension_overrides"].is_object()) {
		json normalized = json::object();
		for (auto it = input["free_dimension_overrides"].begin();
			 it != input["free_dimension_overrides"].end(); ++it) {
			if (!it.value().is_number_integer())
				continue;
			auto dim = it.value().get<int64_t>();
			auto name = it.key();
			if (try_apply([&] { session_options.AddFreeDimensionOverrideByName(name.c_str(), dim); }))
				normalized[name] = dim;
		}
		if (!normalized.empty())
			applied["free_dimension_overrides"] = normalized;
	}

	// config_entries: AddSessionConfigEntry accepts any string key, so we readback each entry
	// via GetSessionConfigEntry to ensure the echo reflects what ORT actually stored.
	if (input.contains("config_entries") && input["config_entries"].is_object()) {
		json normalized = json::object();
		for (auto it = input["config_entries"].begin(); it != input["config_entries"].end(); ++it) {
			std::string sv;
			if (it.value().is_string())
				sv = it.value().get<std::string>();
			else if (it.value().is_boolean())
				sv = it.value().get<bool>() ? "1" : "0";
			else if (it.value().is_number_integer())
				sv = std::to_string(it.value().get<int64_t>());
			else
				continue;
			auto key = it.key();
			if (!try_apply([&] { session_options.AddConfigEntry(key.c_str(), sv.c_str()); }))
				continue;

			size_t needed = 0;
			OrtStatus *st = Ort::GetApi().GetSessionConfigEntry(
				session_options, key.c_str(), nullptr, &needed
			);
			if (st != nullptr) {
				Ort::GetApi().ReleaseStatus(st);
				continue;
			}
			std::string out(needed, '\0');
			st = Ort::GetApi().GetSessionConfigEntry(
				session_options, key.c_str(), out.data(), &needed
			);
			if (st != nullptr) {
				Ort::GetApi().ReleaseStatus(st);
				continue;
			}
			if (!out.empty() && out.back() == '\0')
				out.pop_back();
			normalized[key] = out;
		}
		if (!normalized.empty())
			applied["config_entries"] = normalized;
	}

	return applied;
}

} // namespace

json Orts::onnx::session::collect_extensions(const json &option) {
	json result = json::array();
	if (!option.is_object())
		return result;
	auto add = [&](const std::string &path) {
		for (auto &e : result) {
			if (e.is_string() && e.get<std::string>() == path)
				return;
		}
		result.push_back(path);
	};
	if (option.contains("extensions") && option["extensions"].is_array()) {
		for (auto &e : option["extensions"]) {
			if (e.is_string())
				add(e.get<std::string>());
		}
	}
	if (option.contains("ortextensions_path") && option["ortextensions_path"].is_string())
		add(option["ortextensions_path"].get<std::string>());
	return result;
}

Orts::onnx::session::session(session_key key, const json &option)
	: session_options(), created_at(std::chrono::system_clock::now()), allocator(), key(std::move(key)) {
	_option["cuda"] = false;

	// session-level options (apply before EP/extension registration)
	if (option.contains("session_options") && option["session_options"].is_object()) {
		auto applied = apply_session_options(session_options, option["session_options"]);
		if (!applied.empty())
			_option["session_options"] = applied;
	}

	// register custom op libraries: extensions array + legacy ortextensions_path, deduplicated
	auto extensions = collect_extensions(option);
	if (!extensions.empty()) {
		for (auto &e : extensions)
			register_extension(session_options, e.get<std::string>());
		_option["extensions"] = extensions;
	}

	if (providers::available_providers.has_cuda() && option.contains("cuda") && (
		    !option["cuda"].is_boolean() || option["cuda"].get<bool>())) {
#ifdef HAS_CUDA
		_option["cuda"] = append_cuda_session_options(session_options, option);
#else
		throw runtime_error("CUDA is not supported");
#endif
	}

	if (option.contains("input_shape") && option["input_shape"].is_object())
		_option["input_shape"] = option["input_shape"];
	if (option.contains("output_shape") && option["output_shape"].is_object())
		_option["output_shape"] = option["output_shape"];
}

Orts::onnx::session::session(session_key key, const std::string &path, const json &option)
	: session(std::move(key), option) {
#ifdef _WIN32
	int size_needed = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &wstr[0], size_needed);

	auto model_path = wstr.c_str();
#else
	auto model_path = path.c_str();
#endif

	ort_session = new Ort::Session(env, model_path, session_options);
	init();
}

Orts::onnx::session::session(
	session_key key, const char *model_data, const size_t model_data_length, const json &option
)
	: session(std::move(key), option) {
	ort_session = new Ort::Session(env, model_data, model_data_length, session_options);
	init();
}

void Orts::onnx::session::init() {
	assert(ort_session != nullptr);

	// input metadata
	inputCount = ort_session->GetInputCount();
	for (size_t i = 0; i < inputCount; i++) {
		auto name = ort_session->GetInputNameAllocated(i, allocator);
		_inputNames.emplace_back(name.get());

		auto info = ort_session->GetInputTypeInfo(i);
		auto tensorType = info.GetTensorTypeAndShapeInfo();
		auto shape = tensorType.GetShape();
		_inputs.emplace_back(name.get(), tensorType.GetElementType(), shape);

		override_shape("input_shape", _inputs.back());
	}
	_option.erase("input_shape");
	for (auto &name: _inputNames)
		inputNames.push_back(name.c_str());

	// output metadata
	outputCount = ort_session->GetOutputCount();
	for (size_t i = 0; i < outputCount; i++) {
		auto name = ort_session->GetOutputNameAllocated(i, allocator);
		_outputNames.emplace_back(name.get());

		auto info = ort_session->GetOutputTypeInfo(i);
		auto tensorType = info.GetTensorTypeAndShapeInfo();
		auto shape = tensorType.GetShape();
		_outputs.emplace_back(name.get(), tensorType.GetElementType(), shape);

		override_shape("output_shape", _outputs.back());
	}
	_option.erase("output_shape");
	for (auto &name: _outputNames)
		outputNames.push_back(name.c_str());

	PLOG(L_DEBUG) << "Session created: " << key.model_name << "/" << key.model_version << std::endl;
}

void onnxruntime_server::onnx::session::override_shape(const char *option_key, value_info &value) {
	if (_option.contains(option_key) && _option[option_key].contains(value.name)) {
		auto override_shape = _option[option_key][value.name];
		if (override_shape.is_array()) {
			if (override_shape.size() != value.shape.size())
				throw runtime_error(std::string(option_key) + " override size mismatch: " + override_shape.dump());

			for (auto p = 0; p < value.shape.size(); p++) {
				if (p < override_shape.size())
					value.shape[p] = override_shape[p].get<int64_t>();
			}
		}
	}
}

Orts::onnx::session::~session() {
	delete ort_session;
}

void Orts::onnx::session::touch() {
	last_executed_at = std::chrono::system_clock::now();
	execution_count++;
}

const std::vector<Orts::onnx::value_info> &Orts::onnx::session::inputs() const {
	return _inputs;
}

const std::vector<Orts::onnx::value_info> &Orts::onnx::session::outputs() const {
	return _outputs;
}

std::vector<Ort::Value>
Orts::onnx::session::run(const Ort::MemoryInfo &memory_info, const std::vector<Ort::Value> &input_values) {
	assert(ort_session != nullptr);

	if (input_values.size() != inputCount) {
		throw runtime_error("params size is not same as: " + std::to_string(inputCount));
	}

	Ort::RunOptions options;

	return ort_session->Run(
		options, inputNames.data(), input_values.data(), inputCount, outputNames.data(), outputCount
	);
}

json onnxruntime_server::onnx::session::to_json() const {
	json::object_t dict;
	dict["model"] = key.model_name;
	dict["version"] = key.model_version;
	dict["created_at"] = std::chrono::system_clock::to_time_t(created_at);
	dict["last_executed_at"] = std::chrono::system_clock::to_time_t(last_executed_at);
	dict["execution_count"] = execution_count;

	json::object_t inputs;
	for (auto &input: _inputs) {
		inputs[input.name] = input.type_to_string();
	}
	dict["inputs"] = inputs;

	json::object_t outputs;
	for (auto &output: _outputs) {
		outputs[output.name] = output.type_to_string();
	}
	dict["outputs"] = outputs;
	dict["option"] = _option;

	return dict;
}
