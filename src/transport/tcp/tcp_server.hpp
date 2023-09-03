//
// Created by Kibae Shin on 2023/09/02.
//

#ifndef ONNX_RUNTIME_SERVER_TCP_SERVER_HPP
#define ONNX_RUNTIME_SERVER_TCP_SERVER_HPP

#include "../../onnx_runtime_server.hpp"

namespace onnx_runtime_server::transport::tcp {
	struct protocol_header {
		int16_t type;
		int32_t length;
	} __attribute__((packed));

	class tcp_server;

	class tcp_session : public std::enable_shared_from_this<tcp_session> {
	  private:
		asio::socket socket;
		tcp_server *server;
		char chunk[1024];
		std::string buffer;

	  public:
		tcp_session(asio::socket socket, tcp_server *server);
		~tcp_session();

		void close();
		void do_read();
		void do_task(protocol_header header);

		void do_write(const std::string &buf);
	};

	class tcp_server : public server {
	  private:
		std::list<std::shared_ptr<tcp_session>> sessions;

	  protected:
		void client_connected(asio::socket socket) override;

	  public:
		tcp_server(
			const class config &config, onnx::session_manager *onnx_session_manager, builtin_thread_pool *worker_pool
		);
		void remove_session(const std::shared_ptr<tcp_session> &session);
	};

} // namespace onnx_runtime_server::transport::tcp

#endif // ONNX_RUNTIME_SERVER_TCP_SERVER_HPP
