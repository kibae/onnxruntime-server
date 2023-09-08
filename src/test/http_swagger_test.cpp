//
// Created by kibae on 23. 8. 17.
//

#include "../transport/http/http_server.hpp"
#include "test_common.hpp"

beast::http::response<beast::http::dynamic_body>
http_request(beast::http::verb method, const std::string &target, short port, std::string body, int keep_alive = 0);

TEST(test_onnxruntime_server_http_swagger, HttpSwaggerTest) {
	Orts::config config;
	config.http_port = 0;
	config.swagger_url_path = "/swagger";
	config.model_bin_getter = test_model_bin_getter;

	boost::asio::io_context io_context;
	Orts::onnx::session_manager manager(config.model_bin_getter);
	Orts::builtin_thread_pool worker_pool(config.num_threads);
	Orts::transport::http::http_server server(io_context, config, &manager, &worker_pool);
	std::cout << server.port() << std::endl;

	bool running = true;
	std::thread server_thread([&io_context, &running]() { test_server_run(io_context, &running); });

	TIME_MEASURE_INIT

	{ // health check
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/health", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		ASSERT_EQ(boost::beast::buffers_to_string(res.body().data()), "OK");
	}

	{ // not found
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/not-exists-path", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::not_found);
		ASSERT_EQ(boost::beast::buffers_to_string(res.body().data()), "Not Found");
	}

	{ // Get swagger index.html
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/swagger", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);

		auto body = boost::beast::buffers_to_string(res.body().data());
		ASSERT_GT(body.length(), 300);
	}

	{ // Get swagger index.html
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/swagger/", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);

		auto body = boost::beast::buffers_to_string(res.body().data());
		ASSERT_GT(body.length(), 300);
	}

	{ // Get swagger openapi.yaml
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/swagger/openapi.yaml", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);

		auto body = boost::beast::buffers_to_string(res.body().data());
		ASSERT_GT(body.length(), 300);
	}

	{ // keep alive
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/swagger/", server.port(), "", 3);
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);

		auto body = boost::beast::buffers_to_string(res.body().data());
		ASSERT_GT(body.length(), 300);
	}

	running = false;
	server_thread.join();
}

beast::http::response<beast::http::dynamic_body>
http_request(beast::http::verb method, const std::string &target, short port, std::string body, int keep_alive) {
	boost::asio::io_context ioc;
	boost::asio::ip::tcp::socket socket(ioc);
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
	socket.connect(endpoint);

	beast::http::request<beast::http::string_body> req(method, target, 11);
	req.set(beast::http::field::host, "localhost");
	if (keep_alive > 0)
		req.keep_alive(keep_alive);

	if (!body.empty()) {
		req.set(beast::http::field::content_type, "application/json");
		req.body() = body;
		req.prepare_payload();
	}
	beast::http::write(socket, req);

	boost::beast::flat_buffer buffer;
	beast::http::response<beast::http::dynamic_body> res;

	beast::http::read(socket, buffer, res);

	if (keep_alive > 0)
		sleep(1);

	socket.close();
	return res;
}
