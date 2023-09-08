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

namespace onnxruntime_server::transport::http {
	class swagger_serve {
		std::string swagger_url_path;
		std::shared_ptr<std::string> _cache_index_html = nullptr;
		std::shared_ptr<std::string> _cache_openapi_yaml = nullptr;

	  public:
		swagger_serve(const std::string &swagger_url_path);

		bool is_swagger_url(const std::string &url) const;
		std::shared_ptr<beast::http::response<beast::http::string_body>>
		get_response(const std::string &url, unsigned http_version);
	};

	template <class Session> class http_session_base : public std::enable_shared_from_this<Session> {
	  protected:
		beast::flat_buffer buffer;
		beast::http::request<beast::http::string_body> req;

		virtual onnx::session_manager *get_onnx_session_manager() = 0;
		std::shared_ptr<beast::http::response<beast::http::string_body>> handle_request();

		virtual void do_read() = 0;
		virtual void on_read(beast::error_code ec, std::size_t bytes_transferred) = 0;
		virtual void do_write(std::shared_ptr<beast::http::response<beast::http::string_body>> msg) = 0;

		std::string _remote_endpoint;
		onnxruntime_server::task::benchmark request_time;

	  public:
		http_session_base();

		virtual void run() = 0;
		virtual void close() = 0;
		virtual std::string get_remote_endpoint() = 0;
		virtual swagger_serve &get_swagger() = 0;
	};

	class http_session;

	class http_server : public server {
	  private:
		std::list<std::shared_ptr<http_session>> sessions;

	  protected:
		void client_connected(asio::socket socket) override;

	  public:
		swagger_serve swagger;

		http_server(
			boost::asio::io_context &io_context, const class config &config,
			onnx::session_manager *onnx_session_manager, builtin_thread_pool *worker_pool
		);
		void remove_session(const std::shared_ptr<http_session> &session);
	};

	class http_session : public http_session_base<http_session> {
	  private:
		http_server *server;
		beast::tcp_stream stream;

		onnx::session_manager *get_onnx_session_manager() override;
		void do_read() override;
		void on_read(beast::error_code ec, std::size_t bytes_transferred) override;
		void do_write(std::shared_ptr<beast::http::response<beast::http::string_body>> msg) override;

		std::string get_remote_endpoint() override;

	  public:
		http_session(asio::socket socket, http_server *server);
		~http_session();
		void run() override;
		void close() override;
		swagger_serve &get_swagger() override;
	};

#ifdef HAS_OPENSSL
	class https_session;

	class https_server : public server {
	  private:
		std::list<std::shared_ptr<https_session>> sessions;
		boost::asio::ssl::context ctx;

	  protected:
		void client_connected(asio::socket socket) override;

	  public:
		swagger_serve swagger;

		https_server(
			boost::asio::io_context &io_context, const class config &config,
			onnx::session_manager *onnx_session_manager, builtin_thread_pool *worker_pool
		);
		void remove_session(const std::shared_ptr<https_session> &session);
	};

	class https_session : public http_session_base<https_session> {
	  private:
		https_server *server;
		boost::asio::ssl::stream<asio::socket> stream;

		onnx::session_manager *get_onnx_session_manager() override;
		void do_read() override;
		void on_read(beast::error_code ec, std::size_t bytes_transferred) override;
		void do_write(std::shared_ptr<beast::http::response<beast::http::string_body>> msg) override;

		std::string get_remote_endpoint() override;

	  public:
		https_session(asio::socket socket, https_server *server, boost::asio::ssl::context &ctx);
		~https_session();
		void run() override;
		void close() override;
		swagger_serve &get_swagger() override;
	};

#endif
} // namespace onnxruntime_server::transport::http

#endif // ONNX_RUNTIME_SERVER_HTTP_SERVER_HPP
