#include "http_server.hpp"

#include "http_session_base.cpp"

onnxruntime_server::transport::http::https_session::https_session(
	asio::socket socket, https_server *server, boost::asio::ssl::context &ctx
)
	: http_session_base(), stream(std::move(socket), ctx), server(server) {
	stream.handshake(boost::asio::ssl::stream_base::server);
}

onnxruntime_server::transport::http::https_session::~https_session() {
	try {
		stream.shutdown();
		stream.lowest_layer().close();
	} catch (std::exception &e) {
		LOG(WARNING) << get_remote_endpoint() << " transport::http::https_session::~https_session: " << e.what()
					 << std::endl;
	}
}

void onnxruntime_server::transport::http::https_session::run() {
	boost::asio::dispatch(
		stream.get_executor(), beast::bind_front_handler(&https_session::do_read, shared_from_this())
	);
}

onnxruntime_server::onnx::session_manager *
onnxruntime_server::transport::http::https_session::get_onnx_session_manager() {
	return server->get_onnx_session_manager();
}

void onnxruntime_server::transport::http::https_session::do_read() {
	if (_remote_endpoint.empty())
		_remote_endpoint = stream.lowest_layer().remote_endpoint().address().to_string() + ":" +
						   std::to_string(stream.lowest_layer().remote_endpoint().port());

	req = {};

	beast::http::async_read(
		stream, buffer, req, beast::bind_front_handler(&https_session::on_read, shared_from_this())
	);
}

void onnxruntime_server::transport::http::https_session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
	if (ec == beast::http::error::end_of_stream)
		return close();

	if (ec) {
		LOG(WARNING) << get_remote_endpoint() << " transport::http::https_session::do_read: " << ec.message()
					 << std::endl;
		return close();
	}

	request_time.touch();
	do_write(handle_request());
}

void onnxruntime_server::transport::http::https_session::do_write(
	std::shared_ptr<beast::http::response<beast::http::string_body>> msg
) {
	LOG(INFO, "ACCESS") << get_remote_endpoint() << " task: " << req.method_string() << " " << req.target()
						<< " status: " << msg->result_int() << " duration: " << request_time.get_duration()
						<< std::endl;

	beast::http::async_write(
		stream, *msg,
		[self = shared_from_this(), msg](beast::error_code ec, std::size_t bytes_transferred) {
			if (ec) {
				LOG(WARNING) << self->get_remote_endpoint()
							 << " transport::http::https_session_base::do_write: " << ec.message() << std::endl;
				return self->close();
			}

			if (!self->req.keep_alive())
				return self->close();

			self->do_read();
		}
	);
}

void onnxruntime_server::transport::http::https_session::close() {
	server->remove_session(shared_from_this());
}

std::string onnxruntime_server::transport::http::https_session::get_remote_endpoint() {
	return _remote_endpoint;
}

onnxruntime_server::transport::http::swagger_serve &onnxruntime_server::transport::http::https_session::get_swagger() {
	return server->swagger;
}
