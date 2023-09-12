#include "http_server.hpp"

template <class Session>
onnxruntime_server::transport::http::http_session_base<Session>::http_session_base() : buffer(), req() {
}

#define CONTENT_TYPE_PLAIN_TEXT "text/plain"
#define CONTENT_TYPE_JSON "application/json"

template <class Session>
std::shared_ptr<beast::http::response<beast::http::string_body>>
onnxruntime_server::transport::http::http_session_base<Session>::handle_request() {
	auto const simple_response =
		[this](beast::http::status method, beast::string_view content_type, beast::string_view body) {
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
				auto task = task::execute_session(get_onnx_session_manager(), model, version, req.body());
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}

			// API: Get sessions
			if (req.method() == boost::beast::http::verb::get) {
				auto task = task::get_session(get_onnx_session_manager(), model, version);
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}

			// API: Destroy sessions
			if (req.method() == boost::beast::http::verb::delete_) {
				auto task = task::destroy_session(get_onnx_session_manager(), model, version);
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}
		}

		if (target == "/api/sessions") {
			// API: List sessions
			if (req.method() == boost::beast::http::verb::get) {
				auto task = task::list_session(get_onnx_session_manager());
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}

			// API: Create session
			if (req.method() == boost::beast::http::verb::post) {
				auto task = task::create_session(get_onnx_session_manager(), req.body());
				auto res = task.run();
				return simple_response(beast::http::status::ok, CONTENT_TYPE_JSON, res.dump());
			}
		}

		if (target == "/health")
			return simple_response(beast::http::status::ok, CONTENT_TYPE_PLAIN_TEXT, "OK");

		if (get_swagger().is_swagger_url(target)) {
			auto res = get_swagger().get_response(target, req.version());
			res->keep_alive(req.keep_alive());
			return res;
		}

		return simple_response(beast::http::status::not_found, CONTENT_TYPE_PLAIN_TEXT, "Not Found");
	} catch (Orts::exception &e) {
		PLOG(L_WARNING) << get_remote_endpoint() << " transport::http::http_session_base::handle_request: " << e.what()
						<< std::endl;

		return simple_response(e.status_code, CONTENT_TYPE_JSON, Orts::exception::what_to_json(e.type(), e.what()));
	} catch (std::exception &e) {
		PLOG(L_WARNING) << get_remote_endpoint() << " transport::http::http_session_base::handle_request: " << e.what()
						<< std::endl;

		return simple_response(
			beast::http::status::internal_server_error, CONTENT_TYPE_JSON,
			Orts::exception::what_to_json("runtime_error", e.what())
		);
	}
}
