//
// Created by Kibae Shin on 2023/08/31.
//

#include "../onnx_runtime_server.hpp"

Orts::transport::server::server(
	Orts::onnx::session_manager *onnx_session_manager, Orts::builtin_thread_pool *worker_pool, int port
)
	: io_context(), acceptor(io_context, asio::endpoint(asio::v4(), port)), socket(io_context),
	  onnx_session_manager(onnx_session_manager), worker_pool(worker_pool) {

	assigned_port = acceptor.local_endpoint().port();

	accept();
}

Orts::transport::server::~server() {
	io_context.stop();
	socket.close();
}

void Orts::transport::server::accept() {
	acceptor.async_accept(socket, [this](boost::system::error_code ec) {
		if (!ec) {
			client_connected(std::move(socket));
		}
		accept();
	});
}

void Orts::transport::server::run(bool *running, long long timeout_millis) {
	auto timeout = std::chrono::milliseconds{timeout_millis};
	while (*running) {
		io_context.run_for(timeout);
	}
}

Orts::builtin_thread_pool *Orts::transport::server::get_worker_pool() {
	return worker_pool;
}

Orts::onnx::session_manager *Orts::transport::server::get_onnx_session_manager() {
	return onnx_session_manager;
}

uint_least16_t Orts::transport::server::port() const {
	return assigned_port;
}
