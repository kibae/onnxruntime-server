//
// Created by Kibae Shin on 2023/09/02.
//

#ifndef ONNX_RUNTIME_SERVER_HTTP_SERVER_HPP
#define ONNX_RUNTIME_SERVER_HTTP_SERVER_HPP

#include "../../onnxruntime_server.hpp"

#include <regex>

#define BOOST_ASIO_SEPARATE_COMPILATION
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#ifdef HAS_OPENSSL
#include <boost/beast/ssl.hpp>
#endif

namespace beast = boost::beast;
using boost::system::error_code;

namespace onnxruntime_server::transport::http {
	class swagger_serve {
		std::string swagger_url_path;
		std::shared_ptr<std::string> _cache_index_html = nullptr;
		std::shared_ptr<std::string> _cache_openapi_yaml = nullptr;

	  public:
		explicit swagger_serve(const std::string &swagger_url_path);

		[[nodiscard]] bool is_swagger_url(const std::string &url) const;
		std::shared_ptr<beast::http::response<beast::http::string_body>>
		get_response(const std::string &url, unsigned http_version);
	};

	class http_session_base {
	  protected:
		std::size_t body_limit;
		beast::flat_buffer buffer;

		std::shared_ptr<beast::http::response<beast::http::string_body>> handle_request(
			onnx::session_manager &session_manager, swagger_serve &swagger,
			beast::http::request_parser<beast::http::string_body> &req_parser
		);

		std::string _remote_endpoint;

	  private:
		onnxruntime_server::task::benchmark request_time;

	  public:
		explicit http_session_base(std::size_t body_limit);

		void run(onnx::session_manager &session_manager, swagger_serve &swagger);
		virtual error_code do_read(beast::http::request_parser<beast::http::string_body> &req_parser) = 0;
		virtual error_code do_write(std::shared_ptr<beast::http::response<beast::http::string_body>> msg) = 0;
		virtual std::string get_remote_endpoint() = 0;
	};

	class http_session : public http_session_base {
	  private:
		beast::tcp_stream stream;

		error_code do_read(beast::http::request_parser<beast::http::string_body> &req_parser) override;
		error_code do_write(std::shared_ptr<beast::http::response<beast::http::string_body>> msg) override;
		std::string get_remote_endpoint() override;

	  public:
		http_session(asio::socket socket, size_t body_limit);
	};

	class http_server : public server {
	  protected:
		void client_connected(asio::socket socket) override;

	  public:
		swagger_serve swagger;

		http_server(
			boost::asio::io_context &io_context, const class config &config, onnx::session_manager &onnx_session_manager
		);
	};

#ifdef HAS_OPENSSL
	class https_session : public http_session_base {
	  private:
		boost::asio::ssl::stream<asio::socket> stream;

		error_code do_read(beast::http::request_parser<beast::http::string_body> &req_parser) override;
		error_code do_write(std::shared_ptr<beast::http::response<beast::http::string_body>> msg) override;
		std::string get_remote_endpoint() override;

	  public:
		https_session(asio::socket socket, boost::asio::ssl::context &ctx, size_t body_limit);
	};

	class https_server : public server {
	  private:
		boost::asio::ssl::context ctx;

	  protected:
		void client_connected(asio::socket socket) override;

	  public:
		swagger_serve swagger;

		https_server(
			boost::asio::io_context &io_context, const class config &config, onnx::session_manager &onnx_session_manager
		);
	};

#endif
} // namespace onnxruntime_server::transport::http

#endif // ONNX_RUNTIME_SERVER_HTTP_SERVER_HPP
