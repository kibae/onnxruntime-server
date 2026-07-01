//
// Created by Kibae Shin on 2023/08/31.
//
#include "tcp_server.hpp"

#include <boost/filesystem.hpp>
#include <fstream>

onnxruntime_server::transport::tcp::tcp_session::tcp_session(
	asio::socket socket, long request_payload_limit, int64_t model_upload_limit, std::string model_upload_dir
)
	: socket(std::move(socket)), request_payload_limit(request_payload_limit), model_upload_limit(model_upload_limit),
	  model_upload_dir(std::move(model_upload_dir)) {
	// use heap memory to avoid stack overflow
	chunk.resize(MAX_RECV_BUF_LENGTH);
}

onnxruntime_server::transport::tcp::tcp_session::~tcp_session() {
	cleanup_upload();
}

void onnxruntime_server::transport::tcp::tcp_session::cleanup_upload() {
	if (uploaded_model_path.empty())
		return;

	boost::system::error_code ec;
	boost::filesystem::remove(uploaded_model_path, ec);
	uploaded_model_path.clear();
}

void Orts::transport::tcp::tcp_session::run(onnx::session_manager &session_manager) {
	while (true) {
		auto req = do_read();
		if (!req.has_value())
			break;

		try {
			auto header = req.value();

			auto cstr = buffer.c_str();
			auto json = header.json_length > 0 ? parse_request_json(cstr, cstr + header.json_length) : json::object();

			const char *post = nullptr;
			size_t post_length = 0;
			if (!uploaded_model_path.empty()) {
				// The model binary was streamed to disk (see do_read); load it via the
				// existing option.path branch of session_manager instead of an in-memory
				// buffer, so a large upload never has to reside in RAM.
				if (!json.is_object())
					json = json::object();
				json["option"]["path"] = uploaded_model_path;
			} else {
				post = header.post_length > 0 ? cstr + header.json_length : nullptr;
				post_length = header.post_length;
			}

			// create task
			request_time.touch();
			auto task = create_task(session_manager, header.type, json, post, post_length);
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

bool onnxruntime_server::transport::tcp::tcp_session::read_exact(
	int64_t length, const std::function<void(const char *, size_t)> &sink
) {
	boost::system::error_code ec;
	while (length > 0) {
		std::size_t n =
			socket.read_some(boost::asio::buffer(chunk.data(), NUM_MIN((int64_t)MAX_RECV_BUF_LENGTH, length)), ec);
		if (ec || n == 0)
			return false;
		sink(chunk.data(), n);
		length -= (int64_t)n;
	}
	return true;
}

std::optional<Orts::transport::tcp::protocol_header> Orts::transport::tcp::tcp_session::do_read() {
	buffer.clear();
	cleanup_upload();

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

	// Reject malformed/oversized framing before it drives allocation or is trusted as a
	// read length. The header fields are attacker-controlled signed int64 read off the
	// wire: each byte-length must be non-negative, the JSON body is bounded by
	// request_payload_limit, and the post body by model_upload_limit for a CREATE_SESSION
	// model upload (request_payload_limit otherwise, since no other request type carries a
	// post body). length must equal json_length + post_length, which also bounds the total
	// read (each part is bounded and non-negative, so their sum cannot overflow int64). The
	// HTTP/HTTPS paths already bound the body via Beast body_limit; the TCP path had none.
	const bool is_create = header.type == task::CREATE_SESSION;
	const int64_t post_limit = is_create ? model_upload_limit : request_payload_limit;
	if (header.json_length < 0 || header.post_length < 0 || header.json_length > request_payload_limit ||
		header.post_length > post_limit || header.length != header.json_length + header.post_length) {
		PLOG(L_WARNING) << get_remote_endpoint() << " transport::tcp_session::do_read: invalid protocol header"
						<< std::endl;
		// Tell the client why before the connection is closed, instead of a bare disconnect.
		send_error("bad_request_error", "invalid or oversized protocol header");
		return std::nullopt;
	}

	// JSON body -> in-memory buffer (bounded by request_payload_limit).
	if (!read_exact(header.json_length, [this](const char *p, size_t n) { buffer.append(p, n); }))
		return std::nullopt;

	if (is_create && header.post_length > 0) {
		// Model binary -> temp file, streamed chunk by chunk so it never resides in RAM.
		boost::filesystem::path dir = model_upload_dir.empty() ? boost::filesystem::temp_directory_path()
															   : boost::filesystem::path(model_upload_dir);
		// unique_path yields an unpredictable name, avoiding symlink races in a shared temp dir.
		boost::filesystem::path path =
			dir / boost::filesystem::unique_path("onnxruntime-server-upload-%%%%-%%%%-%%%%-%%%%.onnx");
		uploaded_model_path = path.string();

		std::ofstream ofs(uploaded_model_path, std::ios::binary | std::ios::trunc);
		if (!ofs) {
			PLOG(L_WARNING) << get_remote_endpoint() << " transport::tcp_session::do_read: cannot open upload temp file"
							<< std::endl;
			send_error("runtime_error", "cannot store uploaded model");
			return std::nullopt;
		}
		bool ok = read_exact(header.post_length, [&ofs](const char *p, size_t n) { ofs.write(p, (std::streamsize)n); });
		ofs.close();
		if (!ok || ofs.fail())
			return std::nullopt;
	} else if (header.post_length > 0) {
		// Non-upload post payload -> buffer, contiguous after the JSON body (unchanged layout).
		if (!read_exact(header.post_length, [this](const char *p, size_t n) { buffer.append(p, n); }))
			return std::nullopt;
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
