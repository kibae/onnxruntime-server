//
// Created by Kibae Shin on 2023/09/03.
//

#include "http_server.hpp"

onnx_runtime_server::transport::http::https_server::https_server(
	const onnx_runtime_server::config &config, onnx_runtime_server::onnx::session_manager *onnx_session_manager,
	onnx_runtime_server::builtin_thread_pool *worker_pool
)
	: server(onnx_session_manager, worker_pool, config.https_port), ctx(boost::asio::ssl::context::sslv23) {
	boost::system::error_code ec;
	ctx.set_options(
		boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
			boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 |
			boost::asio::ssl::context::no_tlsv1_1,
		ec
	);
	if (ec)
		throw std::runtime_error("Failed to set SSL context options: " + ec.message());

	ctx.use_certificate_chain_file(config.https_cert, ec);
	if (ec)
		throw std::runtime_error("Failed to load SSL certificate: " + ec.message());

	ctx.use_private_key_file(config.https_key, boost::asio::ssl::context::pem, ec);
	if (ec)
		throw std::runtime_error("Failed to load SSL private key: " + ec.message());
}

void onnx_runtime_server::transport::http::https_server::client_connected(asio::socket socket) {
	auto session = std::make_shared<https_session>(std::move(socket), this, ctx);
	sessions.push_back(session);
	session->run();
}

void onnx_runtime_server::transport::http::https_server::remove_session(const std::shared_ptr<https_session> &session) {
	sessions.remove(session);
}
