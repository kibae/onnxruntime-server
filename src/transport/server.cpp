//
// Created by Kibae Shin on 2023/08/31.
//

#include "../onnxruntime_server.hpp"

Orts::transport::server::server(
	boost::asio::io_context &io_context, Orts::onnx::session_manager &onnx_session_manager, int port,
	long request_payload_limit
)
	: io_context(io_context), acceptor(io_context, asio::endpoint(asio::v4(), port)), socket(io_context),
	  onnx_session_manager(onnx_session_manager), request_payload_limit_(request_payload_limit) {

	assigned_port = acceptor.local_endpoint().port();

	accept();
}

Orts::transport::server::~server() {
	socket.close();
}

void Orts::transport::server::accept() {
	acceptor.async_accept(socket, [this](boost::system::error_code ec) {
		if (!ec) {
			std::thread([this](asio::socket sock) { this->client_connected(std::move(sock)); }, std::move(socket))
				.detach();
		}
		accept();
	});
}

Orts::onnx::session_manager &Orts::transport::server::get_onnx_session_manager() {
	return onnx_session_manager;
}

long Orts::transport::server::request_payload_limit() const {
	return request_payload_limit_;
}

uint_least16_t Orts::transport::server::port() const {
	return assigned_port;
}
