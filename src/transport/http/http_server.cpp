//
// Created by Kibae Shin on 2023/09/03.
//

#include "http_server.hpp"

onnx_runtime_server::transport::http::http_server::http_server(
	const onnx_runtime_server::config &config, onnx_runtime_server::onnx::session_manager *onnx_session_manager,
	onnx_runtime_server::builtin_thread_pool *worker_pool
)
	: server(onnx_session_manager, worker_pool, config.http_port) {
	acceptor.set_option(boost::asio::socket_base::reuse_address(true));
}

void onnx_runtime_server::transport::http::http_server::client_connected(asio::socket socket) {
	auto session = std::make_shared<http_session>(std::move(socket), this);
	sessions.push_back(session);
	session->run();
}

void onnx_runtime_server::transport::http::http_server::remove_session(const std::shared_ptr<http_session> &session) {
	sessions.remove(session);
}
