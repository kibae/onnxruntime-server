//
// Created by Kibae Shin on 2023/09/02.
//

#ifndef ONNX_RUNTIME_SERVER_TCP_SERVER_HPP
#define ONNX_RUNTIME_SERVER_TCP_SERVER_HPP

#include "../../onnxruntime_server.hpp"
#include <optional>

#ifdef WORDS_BIGENDIAN
#define HTONLL(x) (x)
#define NTOHLL(x) (x)
#else
#if WIN32
#define HTONLL(x) ((((uint64_t)htonl(x)) << 32) + htonl((uint64_t)x >> 32))
#define NTOHLL(x) ((((uint64_t)ntohl(x)) << 32) + ntohl((uint64_t)x >> 32))
#else
#define HTONLL(x) ((((uint64_t)htonl(x)) << 32) + htonl(x >> 32))
#define NTOHLL(x) ((((uint64_t)ntohl(x)) << 32) + ntohl(x >> 32))
#endif
#endif

#define MAX_RECV_BUF_LENGTH (1024 * 1024 * 4)

namespace onnxruntime_server::transport::tcp {
	PACKED_STRUCT(protocol_header) {
		int16_t type;
		int64_t length;
		int64_t json_length;
		int64_t post_length;
	};

	class tcp_session {
	  private:
		asio::socket socket;
		std::string chunk;
		std::string buffer;

		onnxruntime_server::task::benchmark request_time;
		std::string _remote_endpoint;

		std::optional<protocol_header> do_read();
		bool do_write(protocol_header &header, const std::string &buf);

		static std::shared_ptr<onnxruntime_server::task::task> create_task(
			onnx::session_manager &onnx_session_manager, int16_t type, const json &request_json, const char *post,
			size_t post_length
		);

	  public:
		explicit tcp_session(asio::socket socket);
		void run(onnx::session_manager &session_manager);

		bool send_error(std::string type, std::string what);
		std::string get_remote_endpoint();
	};

	class tcp_server : public server {
	  protected:
		void client_connected(asio::socket socket) override {
			tcp_session(std::move(socket)).run(get_onnx_session_manager());

			try {
				socket.close();
				PLOG(L_INFO) << "transport::tcp::tcp_server: worker killed" << std::endl;
			} catch (std::exception &e) {
				PLOG(L_WARNING) << "transport::tcp::tcp_server: socket close error " << e.what() << std::endl;
			}
		}

	  public:
		tcp_server(
			boost::asio::io_context &io_context, const class config &config, onnx::session_manager &onnx_session_manager
		)
			: server(io_context, onnx_session_manager, config.tcp_port, config.request_payload_limit) {
			acceptor.set_option(boost::asio::socket_base::reuse_address(true));
		}
	};

} // namespace onnxruntime_server::transport::tcp

#endif // ONNX_RUNTIME_SERVER_TCP_SERVER_HPP
