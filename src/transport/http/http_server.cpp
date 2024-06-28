//
// Created by Kibae Shin on 2023/09/03.
//

#include "http_server.hpp"

onnxruntime_server::transport::http::http_server::http_server(
	boost::asio::io_context &io_context, const onnxruntime_server::config &config,
	onnxruntime_server::onnx::session_manager *onnx_session_manager,
	onnxruntime_server::builtin_thread_pool *worker_pool
)
	: server(io_context, onnx_session_manager, worker_pool, config.http_port, config.request_payload_limit), swagger(config.swagger_url_path) {
	acceptor.set_option(boost::asio::socket_base::reuse_address(true));
}

void onnxruntime_server::transport::http::http_server::client_connected(asio::socket socket) {
	auto session = std::make_shared<http_session>(std::move(socket), this);
	sessions.push_back(session);
	session->run();
}

void onnxruntime_server::transport::http::http_server::remove_session(const std::shared_ptr<http_session> &session) {
	sessions.remove(session);
}
