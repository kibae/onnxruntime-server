//
// Created by kibae on 23. 8. 17.
//

#include "../transport/http/http_server.hpp"
#include "test_common.hpp"

#include <fstream>
#include <sstream>

beast::http::response<beast::http::dynamic_body>
http_request(beast::http::verb method, const std::string &target, short port, std::string body);

TEST(test_onnx_runtime_server_http, HttpServerTest) {
	Orts::config config;
	config.model_bin_getter = test_model_bin_getter;

	Orts::onnx::session_manager manager(config.model_bin_getter);
	Orts::builtin_thread_pool worker_pool(config.num_threads);
	Orts::transport::http::http_server server(config, &manager, &worker_pool);
	ASSERT_EQ(server.port(), config.http_port);

	bool running = true;
	std::thread server_thread([&server, &running]() { server.run(&running, 500); });

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

	{ // API: Create session
		json body = json::parse(R"({"model":"sample","version":1})");
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::post, "/api/sessions", server.port(), body.dump());
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Create session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], 1);
	}

	{ // API: Get session
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/api/sessions/sample/1", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Get session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], 1);
	}

	{ // API: List session
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/api/sessions", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: List sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json.size(), 1);
		ASSERT_EQ(res_json[0]["model"], "sample");
		ASSERT_EQ(res_json[0]["version"], 1);
	}

	{ // API: Execute session
		auto input = json::parse(R"({"x":[[1]],"y":[[2]],"z":[[3]]})");
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::post, "/api/sessions/sample/1", server.port(), input.dump());
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Execute sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json.contains("output"), true);
		ASSERT_EQ(res_json["output"].size(), 1);
		ASSERT_GT(res_json["output"][0], 0);
	}

	{ // API: Destroy session
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::delete_, "/api/sessions/sample/1", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Destroy sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json, true);
	}

	{ // API: List session
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/api/sessions", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: List sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json.size(), 0);
	}

	running = false;
	server_thread.join();
}

beast::http::response<beast::http::dynamic_body>
http_request(beast::http::verb method, const std::string &target, short port, std::string body) {
	boost::asio::io_context ioc;
	boost::asio::ip::tcp::socket socket(ioc);
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
	socket.connect(endpoint);

	beast::http::request<beast::http::string_body> req(method, target, 11);
	req.set(beast::http::field::host, "localhost");
	if (!body.empty()) {
		req.set(beast::http::field::content_type, "application/json");
		req.body() = body;
		req.prepare_payload();
	}
	beast::http::write(socket, req);

	boost::beast::flat_buffer buffer;
	beast::http::response<beast::http::dynamic_body> res;

	beast::http::read(socket, buffer, res);

	socket.close();
	return res;
}
