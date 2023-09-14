//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnxruntime_server.hpp"

Orts::onnx::session_manager::session_manager(model_bin_getter_t model_bin_getter) : model_bin_getter(model_bin_getter) {
	assert(model_bin_getter != nullptr);
}

Orts::onnx::session_manager::~session_manager() {
	for (auto &it : sessions) {
		delete it.second;
	}
}

Orts::onnx::session *
Orts::onnx::session_manager::get_session(const std::string &model_name, const std::string &model_version) {
	auto key = session_key(model_name, model_version);
	return get_session(key);
}

Orts::onnx::session *Orts::onnx::session_manager::get_session(const Orts::onnx::session_key &key) {
	std::lock_guard<std::recursive_mutex> lock(mutex);
	auto it = sessions.find(key);
	if (it == sessions.end())
		return nullptr;
	return it->second;
}

Orts::onnx::session *Orts::onnx::session_manager::create_session(
	const std::string &model_name, const std::string &model_version, const json &option
) {
	return create_session(model_name, model_version, model_bin_getter(model_name, model_version), option);
}

Orts::onnx::session *Orts::onnx::session_manager::create_session(
	const std::string &model_name, const std::string &model_version, const std::string &bin, const json &option
) {
	auto key = session_key(model_name, model_version);
	{
		std::lock_guard<std::recursive_mutex> lock(mutex);

		auto current_session = get_session(key);
		if (current_session != nullptr)
			throw conflict_error("session already exists");

		auto session = new onnx::session(key, bin.data(), bin.size(), option);
		sessions.emplace(key, session);
		return session;
	}
	return nullptr;
}

void Orts::onnx::session_manager::remove_session(const std::string &model_name, const std::string &model_version) {
	auto key = session_key(model_name, model_version);
	remove_session(key);
}

void Orts::onnx::session_manager::remove_session(const Orts::onnx::session_key &key) {
	std::lock_guard<std::recursive_mutex> lock(mutex);
	auto it = sessions.find(key);
	if (it == sessions.end()) {
		throw not_found_error("session not found");
	}
	delete it->second;
	sessions.erase(it);
}
