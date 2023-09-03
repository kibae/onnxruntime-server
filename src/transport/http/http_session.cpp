#include "http_server.hpp"

#include "http_session_base.cpp"

onnx_runtime_server::transport::http::http_session::http_session(
	asio::socket socket, onnx_runtime_server::transport::http::http_server *server
)
	: http_session_base(), stream(std::move(socket)), server(server) {
}

onnx_runtime_server::transport::http::http_session::~http_session() {
	try {
		stream.close();
	} catch (std::exception &e) {
		std::cerr << "onnx_runtime_server::transport::http::http_session::~http_session: " << e.what() << std::endl;
	}
}

void onnx_runtime_server::transport::http::http_session::run() {
	boost::asio::dispatch(stream.get_executor(), beast::bind_front_handler(&http_session::do_read, shared_from_this()));
}

onnx_runtime_server::onnx::session_manager *
onnx_runtime_server::transport::http::http_session::get_onnx_session_manager() {
	return server->get_onnx_session_manager();
}

void onnx_runtime_server::transport::http::http_session::do_read() {
	req = {};
	stream.expires_after(std::chrono::seconds(30));

	beast::http::async_read(stream, buffer, req, beast::bind_front_handler(&http_session::on_read, shared_from_this()));
}

void onnx_runtime_server::transport::http::http_session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
	if (ec == beast::http::error::end_of_stream)
		return close();

	if (ec) {
		std::cerr << "onnx_runtime_server::transport::http::http_session::do_read: " << ec.message() << std::endl;
		return close();
	}

	stream.expires_never();

	do_write(handle_request());
}

void onnx_runtime_server::transport::http::http_session::do_write(beast::http::message_generator &&msg) {
	beast::async_write(stream, std::move(msg), beast::bind_front_handler(&http_session::on_write, shared_from_this()));
}

void onnx_runtime_server::transport::http::http_session::on_write(beast::error_code ec, std::size_t bytes_transferred) {
	if (ec) {
		std::cerr << "onnx_runtime_server::transport::http::http_session_base::do_write: " << ec.message() << std::endl;
		return close();
	}

	if (!req.keep_alive())
		return close();

	do_read();
}

void onnx_runtime_server::transport::http::http_session::close() {
	server->remove_session(shared_from_this());
}
