//
// Created by Kibae Shin on 2023/09/02.
//

#ifndef ONNX_RUNTIME_SERVER_TCP_SERVER_HPP
#define ONNX_RUNTIME_SERVER_TCP_SERVER_HPP

#include "../../onnxruntime_server.hpp"

#ifdef WORDS_BIGENDIAN
#define HTONLL(x) (x)
#define NTOHLL(x) (x)
#else
#define HTONLL(x) ((((uint64_t)htonl(x)) << 32) + htonl(x >> 32))
#define NTOHLL(x) ((((uint64_t)ntohl(x)) << 32) + ntohl(x >> 32))
#endif

#define MAX_RECV_BUF_LENGTH 1024 * 1024 * 4

namespace onnxruntime_server::transport::tcp {
	struct protocol_header {
		int16_t type;
		int64_t length;
		int64_t json_length;
		int64_t post_length;
	} __attribute__((packed));

	class tcp_server;

	class tcp_session : public std::enable_shared_from_this<tcp_session> {
	  private:
		asio::socket socket;
		tcp_server *server;
		std::string chunk;
		std::string buffer;

		onnxruntime_server::task::benchmark request_time;
		std::string _remote_endpoint;

	  public:
		tcp_session(asio::socket socket, tcp_server *server);
		~tcp_session();

		void close();
		void do_read();
		void do_task(protocol_header header);
		void do_write(const std::string &buf);

		void send_error(std::string type, std::string what);

		static std::shared_ptr<onnxruntime_server::task::task> create_task(
			onnx::session_manager *onnx_session_manager, int16_t type, const json &request_json, const char *post,
			size_t post_length
		);

		std::string get_remote_endpoint();
	};

	class tcp_server : public server {
	  private:
		std::list<std::shared_ptr<tcp_session>> sessions;

	  protected:
		void client_connected(asio::socket socket) override;

	  public:
		tcp_server(
			boost::asio::io_context &io_context, const class config &config,
			onnx::session_manager *onnx_session_manager, builtin_thread_pool *worker_pool
		);
		void remove_session(const std::shared_ptr<tcp_session> &session);
	};

} // namespace onnxruntime_server::transport::tcp

#endif // ONNX_RUNTIME_SERVER_TCP_SERVER_HPP
