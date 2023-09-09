//
// Created by Kibae Shin on 2023/09/01.
//

#include <regex>

#include "../onnxruntime_server.hpp"

std::regex space_re(R"(\s+)");
std::regex trim_re(R"(^\s*|\s*$)");

std::string key_rule = R"(([-_a-zA-Z0-9]+):([-_/a-zA-Z0-9]+)(\(([^)]+)\))?)";
std::regex key_re(key_rule);

std::string option_rule = R"(([_a-zA-Z0-9]+)\s*=\s*([^,\s]+))";
std::regex option_re(option_rule);

std::vector<Orts::onnx::session_key_with_option>
onnxruntime_server::onnx::session_key_with_option::parse(const std::string &model_key_list) {
	// model_key_list is a space separated list of model_name:model_version
	std::vector<Orts::onnx::session_key_with_option> models;
	std::string list = std::regex_replace(std::regex_replace(model_key_list, space_re, " "), trim_re, "");
	if (list.empty())
		return models;

	std::smatch keys;
	while (std::regex_search(list, keys, key_re)) {
		json option = json::object();

		// parse option
		auto option_str = keys[4].str();
		if (!option_str.empty()) {
			std::smatch options;
			while (std::regex_search(option_str, options, option_re)) {
				auto option_key = options[1].str();
				auto option_val = options[2].str();

				// cuda option: device_id or true/false
				if (option_key == "cuda") {
					if (option_val == "true" || option_val == "false")
						option[option_key] = option_val == "true";
					else
						option[option_key] = std::stoi(option_val);
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
