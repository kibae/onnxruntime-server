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
		PLOG(L_WARNING) << "transport::tcp::tcp_session::~tcp_session: " << e.what() << std::endl;
	}
}

void onnxruntime_server::transport::tcp::tcp_session::close() {
	server->remove_session(shared_from_this());
}

void Orts::transport::tcp::tcp_session::do_read() {
	if (_remote_endpoint.empty())
		_remote_endpoint =
			socket.remote_endpoint().address().to_string() + ":" + std::to_string(socket.remote_endpoint().port());

	socket.async_read_some(
		boost::asio::buffer(chunk, MAX_RECV_BUF_LENGTH),
		[self = shared_from_this()](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				self->buffer.append(self->chunk, length);

				// check protocol header
				if (self->buffer.size() < sizeof(protocol_header)) {
					self->do_read();
					return;
				}
				protocol_header header = *(protocol_header *)self->buffer.data();
				header.type = ntohs(header.type);
				header.length = NTOHLL(header.length);
				header.json_length = NTOHLL(header.json_length);
				header.post_length = NTOHLL(header.post_length);

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
	request_time.touch();

	// enqueue task to thread pool
	server->get_worker_pool()->enqueue([self = shared_from_this(), header]() {
		try {
			auto cstr = self->buffer.c_str();
			auto json =
				header.json_length > 0
					? json::parse(cstr + sizeof(protocol_header), cstr + sizeof(protocol_header) + header.json_length)
					: json::object();
			auto post = header.post_length > 0 ? cstr + sizeof(protocol_header) + header.json_length : nullptr;

			// create task
			auto task =
				create_task(self->server->get_onnx_session_manager(), header.type, json, post, header.post_length);
			auto result = task->run();

			PLOG(L_INFO, "ACCESS") << self->get_remote_endpoint() << " task: " << task->name()
								   << " duration: " << self->request_time.get_duration() << std::endl;

			auto res_json = result.dump();
			struct protocol_header res_header = {0, 0, 0, 0};
			res_header.type = htons(header.type);
			res_header.json_length = HTONLL(res_json.size());
			res_header.post_length = HTONLL(0);
			res_header.length = res_header.json_length;

			std::string response;
			response.append((char *)&res_header, sizeof(res_header));
			response.append(res_json);
			self->do_write(response);
		} catch (Orts::exception &e) {
			PLOG(L_WARNING) << self->get_remote_endpoint() << " transport::session::do_task: " << e.what() << std::endl;
			self->send_error(e.type(), e.what());
		} catch (std::exception &e) {
			PLOG(L_WARNING) << self->get_remote_endpoint() << " transport::session::do_task: " << e.what() << std::endl;
			self->send_error("runtime_error", e.what());
		}
	});
}

void onnxruntime_server::transport::tcp::tcp_session::send_error(std::string type, std::string what) {
	auto res_json = Orts::exception::what_to_json(type, what);
	struct protocol_header res_header = {0, 0, 0, 0};
	res_header.type = htons(-1);
	res_header.json_length = HTONLL(res_json.size());
	res_header.post_length = HTONLL(0);
	res_header.length = res_header.json_length;

	std::string response;
	response.append((char *)&res_header, sizeof(res_header));
	response.append(res_json);
	do_write(response);
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

std::shared_ptr<Orts::task::task> onnxruntime_server::transport::tcp::tcp_session::create_task(
	onnx::session_manager *onnx_session_manager, int16_t type, const json &request_json, const char *post,
	size_t post_length
) {
	switch (type) {
	case Orts::task::EXECUTE_SESSION:
		return std::make_shared<Orts::task::execute_session>(onnx_session_manager, request_json);
	case Orts::task::GET_SESSION:
		return std::make_shared<Orts::task::get_session>(onnx_session_manager, request_json);
	case Orts::task::CREATE_SESSION:
		return std::make_shared<Orts::task::create_session>(onnx_session_manager, request_json, post, post_length);
	case Orts::task::DESTROY_SESSION:
		return std::make_shared<Orts::task::destroy_session>(onnx_session_manager, request_json);
	case Orts::task::LIST_SESSION:
		return std::make_shared<Orts::task::list_session>(onnx_session_manager);
	default:
		throw bad_request_error("Invalid task type");
	}
}

std::string onnxruntime_server::transport::tcp::tcp_session::get_remote_endpoint() {
	return _remote_endpoint;
}
