//
// Created by Kibae Shin on 2023/08/31.
//
#include "tcp_server.hpp"

onnxruntime_server::transport::tcp::tcp_session::tcp_session(asio::socket socket) : socket(std::move(socket)) {
	// use heap memory to avoid stack overflow
	chunk.resize(MAX_RECV_BUF_LENGTH);
}

void Orts::transport::tcp::tcp_session::run(onnx::session_manager &session_manager) {
	while (true) {
		auto req = do_read();
		if (!req.has_value())
			break;

		try {
			auto header = req.value();

			auto cstr = buffer.c_str();
			auto json = header.json_length > 0 ? json::parse(cstr, cstr + header.json_length) : json::object();
			auto post = header.post_length > 0 ? cstr + header.json_length : nullptr;

			// create task
			request_time.touch();
			auto task = create_task(session_manager, header.type, json, post, header.post_length);
			auto result = task->run();

			PLOG(L_INFO, "ACCESS") << get_remote_endpoint() << " task: " << task->name()
								   << " duration: " << request_time.get_duration() << std::endl;

			auto res_json = result.dump();
			protocol_header res_header = {0, 0, 0, 0};
			res_header.type = htons(header.type);
			res_header.json_length = HTONLL(res_json.size());
			res_header.post_length = HTONLL(0);
			res_header.length = res_header.json_length;

			if (!do_write(res_header, res_json))
				break;
		} catch (Orts::exception &e) {
			PLOG(L_WARNING) << get_remote_endpoint() << " transport::tcp_session::execute: " << e.what() << std::endl;
			send_error(e.type(), e.what());
		} catch (std::exception &e) {
			PLOG(L_WARNING) << get_remote_endpoint() << " transport::tcp_session::execute: " << e.what() << std::endl;
			send_error("runtime_error", e.what());
		}
	}
}

std::optional<Orts::transport::tcp::protocol_header> Orts::transport::tcp::tcp_session::do_read() {
	buffer.clear();

	boost::system::error_code ec;

	// process header
	protocol_header header = {0, 0, 0, 0};
	std::size_t length = socket.read_some(boost::asio::buffer(&header, sizeof(protocol_header)), ec);
	if (length < sizeof(protocol_header) || ec.value())
		return std::nullopt;

	header.type = ntohs(header.type);
	header.length = NTOHLL(header.length);
	header.json_length = NTOHLL(header.json_length);
	header.post_length = NTOHLL(header.post_length);

	while (buffer.size() < header.length) {
		length = socket.read_some(
			boost::asio::buffer(chunk.data(), NUM_MIN(MAX_RECV_BUF_LENGTH, header.length - buffer.length())), ec
		);
		if (ec)
			return std::nullopt;

		buffer.append(chunk.data(), length);
	}
	return header;
}

#undef MAX_LENGTH
#undef MAX_BUFFER_LIMIT

bool onnxruntime_server::transport::tcp::tcp_session::send_error(std::string type, std::string what) {
	auto res_json = Orts::exception::what_to_json(type, what);
	struct protocol_header res_header = {0, 0, 0, 0};
	res_header.type = htons(-1);
	res_header.json_length = HTONLL(res_json.size());
	res_header.post_length = HTONLL(0);
	res_header.length = res_header.json_length;

	return do_write(res_header, res_json);
}

bool Orts::transport::tcp::tcp_session::do_write(
	Orts::transport::tcp::protocol_header &header, const std::string &buf
) {
	boost::system::error_code ec;
	std::size_t sent = socket.write_some(
		std::array<boost::asio::const_buffer, 2>{
			boost::asio::buffer(&header, sizeof(Orts::transport::tcp::protocol_header)), boost::asio::buffer(buf)
		},
		ec
	);
	return !ec && sent > 0;
}

std::shared_ptr<Orts::task::task> onnxruntime_server::transport::tcp::tcp_session::create_task(
	onnx::session_manager &onnx_session_manager, int16_t type, const json &request_json, const char *post,
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
	if (_remote_endpoint.empty())
		_remote_endpoint =
			socket.remote_endpoint().address().to_string() + ":" + std::to_string(socket.remote_endpoint().port());

	return _remote_endpoint;
}
