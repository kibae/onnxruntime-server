//
// Created by Kibae Shin on 2023/09/08.
//

#ifndef ONNXRUNTIME_SERVER_EXCEPTIONS_HPP
#define ONNXRUNTIME_SERVER_EXCEPTIONS_HPP

#include <boost/beast/http.hpp>

#include "json.hpp"

using json = nlohmann::json;
namespace http = boost::beast::http;

namespace onnxruntime_server {
	class exception : public std::runtime_error {
	  public:
		http::status status_code;

		explicit exception(http::status status_code, const std::string &what_arg)
			: std::runtime_error(what_arg), status_code(status_code) {
		}

		virtual std::string type() = 0;

		static std::string what_to_json(std::string type, std::string what) {
			return json::object({
									{"error_type", type},
									{"error", what},
								})
				.dump();
		}
	};

	class not_found_error : public exception {
	  public:
		explicit not_found_error(const std::string &what_arg) : exception(http::status::not_found, what_arg) {
		}

		std::string type() override {
			return "not_found_error";
		}
	};

	class bad_request_error : public exception {
	  public:
		explicit bad_request_error(const std::string &what_arg) : exception(http::status::bad_request, what_arg) {
		}

		std::string type() override {
			return "bad_request_error";
		}
	};

	class conflict_error : public exception {
	  public:
		explicit conflict_error(const std::string &what_arg) : exception(http::status::conflict, what_arg) {
		}

		std::string type() override {
			return "conflict_error";
		}
	};

	class runtime_error : public exception {
	  public:
		explicit runtime_error(const std::string &what_arg) : exception(http::status::internal_server_error, what_arg) {
		}

		std::string type() override {
			return "runtime_error";
		}
	};
} // namespace onnxruntime_server

#endif // ONNXRUNTIME_SERVER_EXCEPTIONS_HPP
