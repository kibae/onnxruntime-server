#include "http_server.hpp"

onnxruntime_server::transport::http::http_session::http_session(asio::socket socket, size_t body_limit)
	: http_session_base(body_limit), stream(std::move(socket)) {
	stream.expires_never();
}

error_code onnxruntime_server::transport::http::http_session::do_read(
	beast::http::request_parser<beast::http::string_body> &req_parser
) {
	boost::system::error_code ec;

	buffer.clear();
	req_parser.body_limit(body_limit);

	beast::http::read(stream, buffer, req_parser, ec);
	return ec;
}

boost::system::error_code onnxruntime_server::transport::http::http_session::do_write(
	std::shared_ptr<beast::http::response<beast::http::string_body>> msg
) {
	boost::system::error_code ec;
	beast::http::write(stream, *msg, ec);
	return ec;
}

std::string onnxruntime_server::transport::http::http_session::get_remote_endpoint() {
	if (_remote_endpoint.empty())
		_remote_endpoint = stream.socket().remote_endpoint().address().to_string() + ":" +
						   std::to_string(stream.socket().remote_endpoint().port());

	return _remote_endpoint;
}
