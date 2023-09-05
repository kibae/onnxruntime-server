//
// Created by Kibae Shin on 2023/09/03.
//
#include "tcp_server.hpp"

onnxruntime_server::transport::tcp::tcp_server::tcp_server(
	const onnxruntime_server::config &config, onnxruntime_server::onnx::session_manager *onnx_session_manager,
	onnxruntime_server::builtin_thread_pool *worker_pool
)
	: server(onnx_session_manager, worker_pool, config.tcp_port) {
	acceptor.set_option(boost::asio::socket_base::reuse_address(true));
}

void onnxruntime_server::transport::tcp::tcp_server::client_connected(asio::socket socket) {
	auto session = std::make_shared<tcp_session>(std::move(socket), this);
	sessions.push_back(session);
	session->do_read();
}

void onnxruntime_server::transport::tcp::tcp_server::remove_session(const std::shared_ptr<tcp_session> &session) {
	sessions.remove(session);
}
