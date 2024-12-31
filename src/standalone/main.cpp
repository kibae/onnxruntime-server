//
// Created by Kibae Shin on 2023/09/06.
//

#include "standalone.hpp"

#include "../transport/http/http_server.hpp"
#include "../transport/tcp/tcp_server.hpp"

bool terminated = false;

void on_sigterm(int) {
	terminated = true;
}

int main(int argc, char *argv[]) {
	onnxruntime_server::standalone server;

	// config
	if (server.init_config(argc, argv) != 0) {
		return 1;
	}

	{ // scope
		boost::asio::io_context io_context;
		Orts::onnx::session_manager manager(server.config.model_bin_getter);

		try {
			server.prepare_models(manager);
		} catch (std::exception &e) {
			PLOG(L_FATAL) << "Failed to prepare models: " << e.what() << std::endl;
			return 1;
		}

		std::vector<std::shared_ptr<Orts::transport::server>> servers;

		if (server.config.use_tcp) {
			servers.emplace_back(std::make_shared<Orts::transport::tcp::tcp_server>(io_context, server.config, manager)
			);
			PLOG(L_INFO) << "TCP Server ready on port: " << server.config.tcp_port << std::endl;
		}

		if (server.config.use_http) {
			servers.emplace_back(
				std::make_shared<Orts::transport::http::http_server>(io_context, server.config, manager)
			);
			PLOG(L_INFO) << "HTTP Server ready on port: " << server.config.http_port << std::endl;
		}

#ifdef HAS_OPENSSL
		if (server.config.use_https) {
			servers.emplace_back(
				std::make_shared<Orts::transport::http::https_server>(io_context, server.config, manager)
			);
			PLOG(L_INFO) << "HTTPS Server ready on port: " << server.config.https_port << std::endl;
		}
#endif

		signal(SIGTERM, on_sigterm);
		signal(SIGPIPE, SIG_IGN);

		auto timeout = std::chrono::milliseconds{1000};
		while (!terminated) {
			io_context.run_for(timeout);
		}

		// cleanup
		servers.clear();
	}

	PLOG(L_INFO) << "Terminated" << std::endl;

	return 0;
}
