//
// Created by Kibae Shin on 2023/09/01.
//

#include <regex>

#include "../onnxruntime_server.hpp"

namespace {

std::regex space_re(R"(\s+)");
std::regex trim_re(R"(^\s*|\s*$)");

std::string key_rule = R"(([-_a-zA-Z0-9]+):([-_/a-zA-Z0-9]+)(\(([^)]+)\))?)";
std::regex key_re(key_rule);

// option key supports dotted notation (e.g. cuda.device_id, session_options.intra_op_num_threads)
std::string option_rule = R"(([_a-zA-Z0-9][_a-zA-Z0-9.]*)\s*=\s*([^,\s]+))";
std::regex option_re(option_rule);

std::regex int_re(R"(^-?[0-9]+$)");

const std::string EXTENSIONS_KEY = "extensions";
const std::string LEGACY_EXTENSION_KEY = "ortextensions_path";

json infer_value(const std::string &raw) {
	if (raw == "true")
		return true;
	if (raw == "false")
		return false;
	if (std::regex_match(raw, int_re)) {
		try {
			return json(std::stoll(raw));
		} catch (...) {
			return raw;
		}
	}
	return raw;
}

std::vector<std::string> split_dot(const std::string &k) {
	std::vector<std::string> parts;
	std::string cur;
	for (char c : k) {
		if (c == '.') {
			if (!cur.empty())
				parts.push_back(cur);
			cur.clear();
		} else {
			cur += c;
		}
	}
	if (!cur.empty())
		parts.push_back(cur);
	return parts;
}

void set_nested(json &option, const std::vector<std::string> &path, const json &value) {
	json *cur = &option;
	for (size_t i = 0; i + 1 < path.size(); ++i) {
		if (!cur->is_object())
			*cur = json::object();
		if (!cur->contains(path[i]) || !(*cur)[path[i]].is_object())
			(*cur)[path[i]] = json::object();
		cur = &(*cur)[path[i]];
	}
	if (!cur->is_object())
		*cur = json::object();
	(*cur)[path.back()] = value;
}

void append_extension(json &option, const std::string &path) {
	if (!option.contains(EXTENSIONS_KEY) || !option[EXTENSIONS_KEY].is_array())
		option[EXTENSIONS_KEY] = json::array();
	auto &arr = option[EXTENSIONS_KEY];
	for (auto &e : arr) {
		if (e.is_string() && e.get<std::string>() == path)
			return;
	}
	arr.push_back(path);
}

} // namespace

std::vector<Orts::onnx::session_key_with_option>
onnxruntime_server::onnx::session_key_with_option::parse(const std::string &model_key_list) {
	// model_key_list is a space separated list of model_name:model_version[(opt1=val1, opt2=val2)]
	// option keys may be dotted (cuda.device_id, session_options.intra_op_num_threads) producing nested objects.
	// extensions/ortextensions_path keys accumulate into an "extensions" array (deduplicated).
	// option entries that don't match the grammar are silently skipped.
	std::vector<Orts::onnx::session_key_with_option> models;
	std::string list = std::regex_replace(std::regex_replace(model_key_list, space_re, " "), trim_re, "");
	if (list.empty())
		return models;

	std::smatch keys;
	while (std::regex_search(list, keys, key_re)) {
		json option = json::object();

		auto option_str = keys[4].str();
		if (!option_str.empty()) {
			std::smatch options;
			while (std::regex_search(option_str, options, option_re)) {
				auto raw_key = options[1].str();
				auto raw_val = options[2].str();
				auto value = infer_value(raw_val);

				auto parts = split_dot(raw_key);
				if (parts.empty()) {
					option_str = options.suffix().str();
					continue;
				}

				if (parts.size() == 1 &&
					(parts[0] == EXTENSIONS_KEY || parts[0] == LEGACY_EXTENSION_KEY) &&
					value.is_string()) {
					append_extension(option, value.get<std::string>());
				} else {
					set_nested(option, parts, value);
				}

				option_str = options.suffix().str();
			}
		}

		models.emplace_back(keys[1].str(), keys[2].str(), option);

		list = keys.suffix().str();
	}

	if (!list.empty())
		throw runtime_error("Invalid model key list: " + model_key_list);

	return models;
}
