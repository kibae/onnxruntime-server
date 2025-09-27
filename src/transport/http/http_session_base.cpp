#include "http_server.hpp"

onnxruntime_server::transport::http::http_session_base::http_session_base(std::size_t body_limit)
	: buffer(), body_limit(body_limit) {
}

#define CONTENT_TYPE_PLAIN_TEXT "text/plain"
#define CONTENT_TYPE_JSON "application/json"

void onnxruntime_server::transport::http::http_session_base::run(
	onnxruntime_server::onnx::session_manager &session_manager,
	onnxruntime_server::transport::http::swagger_serve &swagger
) {
	while (true) {
		beast::http::request_parser<beast::http::string_body> req_parser;
		boost::system::error_code ec = do_read(req_parser);
		if (ec == beast::http::error::end_of_stream)
			break;
		else if (ec) {
			PLOG(L_WARNING) << get_remote_endpoint() << " transport::http::do_read: " << ec.message() << std::endl;
			break;
		}

		auto req = req_parser.get();
		request_time.touch();
		auto res = handle_request(session_manager, swagger, req_parser);
		PLOG(L_INFO, "ACCESS") << get_remote_endpoint() << " task: " << req.method_string() << " " << req.target()
							   << " status: " << res->result_int() << " duration: " << request_time.get_duration()
							   << std::endl;

		ec = do_write(res);
		if (ec) {
			PLOG(L_WARNING) << get_remote_endpoint() << " transport::http::do_write: " << ec.message() << std::endl;
			break;
		}

		if (!req.keep_alive())
			break;
	}
}

std::shared_ptr<beast::http::response<beast::http::string_body>>
onnxruntime_server::transport::http::http_session_base::handle_request(
	onnx::session_manager &session_manager, swagger_serve &swagger,
	beast::http::request_parser<beast::http::string_body> &req_parser
) {
	auto req = req_parser.get();

	auto const simple_response =
		[req](beast::http::status method, beast::string_view content_type, beast::string_view body) {
			auto res = std::make_shared<beast::http::response<beast::http::string_body>>(method, req.version());
			res->set(beast::http::field::content_type, content_type);
			res->keep_alive(req.keep_alive());
			res->body() = std::string(body);
			res->prepare_payload();
			return res;
		};

	try {
		auto target = std::string(req.target());

		std::regex re(R"(/api/sessions/([^/]+)/([^/]+))");
		std::smatch match;
		if ((req.method() == boost::beast::http::verb::get || req.method() == boost::beast::http::verb::post ||
			 req.method() == boost::beast::http::verb::delete_) &&
			std::regex_match(target, match, re) && match.size() > 1) {
			auto model = match[1].str();
			auto version = match[2].str();

			// API: Execute sessions
			if (req.method() == boost::beast::http::verb::post) {
				auto task = task::execute_session(session_manager, model, version, json::parse(req.body()));
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}

			// API: Get sessions
			if (req.method() == boost::beast::http::verb::get) {
				auto task = task::get_session(session_manager, model, version);
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}

			// API: Destroy sessions
			if (req.method() == boost::beast::http::verb::delete_) {
				auto task = task::destroy_session(session_manager, model, version);
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}
		}

		if (target == "/api/sessions") {
			// API: List sessions
			if (req.method() == boost::beast::http::verb::get) {
				auto task = task::list_session(session_manager);
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}

			// API: Create session
			if (req.method() == boost::beast::http::verb::post) {
				auto task = task::create_session(session_manager, json::parse(req.body()));
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}
		}

		if (target == "/api/version" && req.method() == boost::beast::http::verb::get) {
			// API: Get version
			auto version = onnxruntime_server::onnx::version();
			return simple_response(beast::http::status::ok, CONTENT_TYPE_PLAIN_TEXT, version);
		}

		if (target == "/health")
			return simple_response(beast::http::status::ok, CONTENT_TYPE_PLAIN_TEXT, "OK");

		if (swagger.is_swagger_url(target)) {
			auto res = swagger.get_response(target, req.version());
			res->keep_alive(req.keep_alive());
			return res;
		}

		return simple_response(beast::http::status::not_found, CONTENT_TYPE_PLAIN_TEXT, "Not Found");
	} catch (Orts::exception &e) {
		PLOG(L_WARNING) << get_remote_endpoint() << " transport::http::handle_request: " << e.what() << std::endl;

		return simple_response(e.status_code, CONTENT_TYPE_JSON, Orts::exception::what_to_json(e.type(), e.what()));
	} catch (std::exception &e) {
		PLOG(L_WARNING) << get_remote_endpoint() << " transport::http::handle_request: " << e.what() << std::endl;

		return simple_response(
			beast::http::status::internal_server_error, CONTENT_TYPE_JSON,
			Orts::exception::what_to_json("runtime_error", e.what())
		);
	}
}
