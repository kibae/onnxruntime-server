//
// Created by Kibae Shin on 2023/09/03.
//

#include "http_server.hpp"

Orts::transport::http::https_server::https_server(
	boost::asio::io_context &io_context, const onnxruntime_server::config &config,
	Orts::onnx::session_manager &onnx_session_manager
)
	: server(io_context, onnx_session_manager, config.https_port, config.request_payload_limit),
	  ctx(boost::asio::ssl::context::sslv23), swagger(config.swagger_url_path) {
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

void onnxruntime_server::transport::http::https_server::client_connected(asio::socket socket) {
	https_session(std::move(socket), ctx, request_payload_limit()).run(get_onnx_session_manager(), swagger);

	try {
		socket.close();
		PLOG(L_INFO) << "transport::https::https_server: worker killed" << std::endl;
	} catch (std::exception &e) {
		PLOG(L_WARNING) << "transport::http::https_server: socket close error " << e.what() << std::endl;
	}
}
