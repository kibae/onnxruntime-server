//
// Created by Kibae Shin on 2023/08/31.
//
#include "tcp_server.hpp"

onnxruntime_server::transport::tcp::tcp_session::tcp_session(asio::socket socket, tcp_server *server)
	: socket(std::move(socket)), server(server) {
}

onnxruntime_server::transport::tcp::tcp_session::~tcp_session() {
	try {
		socket.close();
	} catch (std::exception &e) {
		std::cerr << "onnxruntime_server::transport::tcp::tcp_session::~tcp_session: " << e.what() << std::endl;
	}
}

void onnxruntime_server::transport::tcp::tcp_session::close() {
	server->remove_session(shared_from_this());
}

#define MAX_LENGTH 1024
#define MAX_BUFFER_LIMIT (1024 * 1024 * 100)

void Orts::transport::tcp::tcp_session::do_read() {
	socket.async_read_some(
		boost::asio::buffer(chunk, MAX_LENGTH),
		[self = shared_from_this()](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				self->buffer.append(self->chunk, length);

				// check protocol header
				if (self->buffer.size() < sizeof(protocol_header)) {
					self->do_read();
					return;
				}
				protocol_header header = *(protocol_header *)self->buffer.data();
				header.type = htons(header.type);
				header.length = htonl(header.length);

				// check buffer size
				if (header.length > MAX_BUFFER_LIMIT) {
					std::cerr << "onnxruntime_server::transport::session::do_read: Buffer size is too large: "
							  << header.length << std::endl;
					self->close();
					return;
				}

				// continue to read
				if (self->buffer.size() < sizeof(protocol_header) + header.length) {
					self->do_read();
					return;
				}

				self->do_task(header);
			} else {
				// close session
				self->close();
			}
		}
	);
}

#undef MAX_LENGTH
#undef MAX_BUFFER_LIMIT

void Orts::transport::tcp::tcp_session::do_task(Orts::transport::tcp::protocol_header header) {
	// enqueue task to thread pool
	server->get_worker_pool()->enqueue([self = shared_from_this(), header]() {
		try {
			// create task
			auto task = Orts::task::create(
				self->server->get_onnx_session_manager(), header.type,
				self->buffer.substr(sizeof(protocol_header), header.length)
			);
			auto result = task->run();
			auto res_json = result.dump();
			struct protocol_header res_header = {0, 0};
			res_header.type = htons(header.type);
			res_header.length = htonl((int32_t)res_json.size());

			std::string response;
			response.append((char *)&res_header, sizeof(res_header));
			response.append(res_json);
			self->do_write(response);
		} catch (std::exception &e) {
			std::cerr << "onnxruntime_server::transport::session::do_read: " << e.what() << std::endl;

			auto res_json = json({{"error", std::string(e.what())}}).dump();
			struct protocol_header res_header = {0, 0};
			res_header.type = htons(header.type);
			res_header.length = htonl((int32_t)res_json.size());

			std::string response;
			response.append((char *)&res_header, sizeof(res_header));
			response.append(res_json);
			self->do_write(response);
		}
	});
}

void onnxruntime_server::transport::tcp::tcp_session::do_write(const std::string &buf) {
	socket.async_write_some(
		boost::asio::buffer(buf),
		[self = shared_from_this()](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				self->buffer.clear();
				self->do_read();
			} else {
				// close session
				self->close();
			}
		}
	);
}
