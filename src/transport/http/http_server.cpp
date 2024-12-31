#include "http_server.hpp"

onnxruntime_server::transport::http::http_server::http_server(
	boost::asio::io_context &io_context, const onnxruntime_server::config &config,
	onnxruntime_server::onnx::session_manager &onnx_session_manager
)
	: server(io_context, onnx_session_manager, config.http_port, config.request_payload_limit),
	  swagger(config.swagger_url_path) {
	acceptor.set_option(boost::asio::socket_base::reuse_address(true));
}

void onnxruntime_server::transport::http::http_server::client_connected(asio::socket socket) {
	http_session(std::move(socket), request_payload_limit()).run(get_onnx_session_manager(), swagger);

	try {
		socket.close();
		PLOG(L_INFO) << "transport::http::http_server: worker killed" << std::endl;
	} catch (std::exception &e) {
		PLOG(L_WARNING) << "transport::http::http_server: socket close error " << e.what() << std::endl;
	}
}
