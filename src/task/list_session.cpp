//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnx_runtime_server.hpp"

Orts::task::list_session::list_session(onnx::session_manager *onnx_session_manager)
	: onnx_session_manager(onnx_session_manager) {
}

json Orts::task::list_session::run() {
	auto sessions = onnx_session_manager->get_sessions();

	json::array_t session_list;
	for (auto &it : sessions) {
		session_list.emplace_back(it.second->to_json());
	}

	return session_list;
}
