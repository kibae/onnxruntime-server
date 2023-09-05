#include "http_server.hpp"

#include "http_session_base.cpp"

onnx_runtime_server::transport::http::https_session::https_session(
	asio::socket socket, https_server *server, boost::asio::ssl::context &ctx
)
	: http_session_base(), stream(std::move(socket), ctx), server(server) {
	stream.handshake(boost::asio::ssl::stream_base::server);
}

onnx_runtime_server::transport::http::https_session::~https_session() {
	try {
		stream.shutdown();
		stream.lowest_layer().close();
	} catch (std::exception &e) {
		std::cerr << "onnx_runtime_server::transport::http::https_session::~https_session: " << e.what() << std::endl;
	}
}

void onnx_runtime_server::transport::http::https_session::run() {
	boost::asio::dispatch(
		stream.get_executor(), beast::bind_front_handler(&https_session::do_read, shared_from_this())
	);
}

onnx_runtime_server::onnx::session_manager *
onnx_runtime_server::transport::http::https_session::get_onnx_session_manager() {
	return server->get_onnx_session_manager();
}

void onnx_runtime_server::transport::http::https_session::do_read() {
	req = {};

	beast::http::async_read(
		stream, buffer, req, beast::bind_front_handler(&https_session::on_read, shared_from_this())
	);
}

void onnx_runtime_server::transport::http::https_session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
	if (ec == beast::http::error::end_of_stream)
		return close();

	if (ec) {
		std::cerr << "onnx_runtime_server::transport::http::https_session::do_read: " << ec.message() << std::endl;
		return close();
	}

	do_write(handle_request());
}

void onnx_runtime_server::transport::http::https_session::do_write(
	std::shared_ptr<beast::http::response<beast::http::string_body>> msg
) {
	beast::http::async_write(
		stream, *msg,
		[self = shared_from_this(), msg](beast::error_code ec, std::size_t bytes_transferred) {
			if (ec) {
				std::cerr << "onnx_runtime_server::transport::http::https_session_base::do_write: " << ec.message()
						  << std::endl;
				return self->close();
			}

			if (!self->req.keep_alive())
				return self->close();

			self->do_read();
		}
	);
}

void onnx_runtime_server::transport::http::https_session::close() {
	server->remove_session(shared_from_this());
}
