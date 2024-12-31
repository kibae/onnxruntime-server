//
// Created by kibae on 23. 8. 17.
//

#include "../../transport/http/http_server.hpp"
#include "../test_common.hpp"

beast::http::response<beast::http::dynamic_body>
http_request(beast::http::verb method, const std::string &target, short port, std::string body);

TEST(test_onnxruntime_server_http, HttpsServerTest) {
	Orts::config config;
	config.https_port = 0;
	config.https_cert = (test_dir / "ssl" / "server-cert.pem").string();
	config.https_key = (test_dir / "ssl" / "server-key.pem").string();
	config.model_bin_getter = test_model_bin_getter;

	boost::asio::io_context io_context;
	Orts::onnx::session_manager manager(config.model_bin_getter);
	Orts::transport::http::https_server server(io_context, config, manager);

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

	{ // API: Create session
		json body = json::parse(R"({"model":"sample","version":"1"})");
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::post, "/api/sessions", server.port(), body.dump());
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Create session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], "1");
	}

	{ // API: Get session
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::get, "/api/sessions/sample/1", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Get session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], "1");
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
		ASSERT_EQ(res_json[0]["version"], "1");
	}

	{ // API: Execute session
		auto input = json::parse(R"({"x":[[1]],"y":[[2]],"z":[[3]]})");
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::post, "/api/sessions/sample/1", server.port(), input.dump());
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Execute sessions\n" << res_json.dump(2) << "\n";
		ASSERT_TRUE(res_json.contains("output"));
		ASSERT_EQ(res_json["output"].size(), 1);
		ASSERT_GT(res_json["output"][0], 0);
	}

	{ // API: Execute session large request
		auto input = json::parse(R"({"x":[[1]],"y":[[2]],"z":[[3]]})");
		int size = 1000000;
		for (int i = 0; i < size; i++) {
			input["x"].push_back(input["x"][0]);
			input["y"].push_back(input["y"][0]);
			input["z"].push_back(input["z"][0]);
		}
		std::cout << input.dump().length() << " bytes\n";

		bool exception = false;
		try {

			TIME_MEASURE_START
			auto res =
				http_request(boost::beast::http::verb::post, "/api/sessions/sample/1", server.port(), input.dump());
			TIME_MEASURE_STOP
		} catch (std::exception &e) {
			exception = true;
			std::cout << e.what() << std::endl;
		}
		ASSERT_TRUE(exception);
	}

	{ // API: Destroy session
		TIME_MEASURE_START
		auto res = http_request(boost::beast::http::verb::delete_, "/api/sessions/sample/1", server.port(), "");
		TIME_MEASURE_STOP
		ASSERT_EQ(res.result(), boost::beast::http::status::ok);
		json res_json = json::parse(boost::beast::buffers_to_string(res.body().data()));
		std::cout << "API: Destroy sessions\n" << res_json.dump(2) << "\n";
		ASSERT_TRUE(res_json);
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

	boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23);
	ssl_context.set_default_verify_paths();
	ssl_context.set_verify_mode(boost::asio::ssl::verify_none);

	boost::asio::ssl::stream<asio::tcp::socket> stream(ioc, ssl_context);
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), port);
	stream.lowest_layer().connect(endpoint);
	boost::system::error_code ec;
	stream.handshake(boost::asio::ssl::stream_base::client, ec);
	if (ec)
		throw std::runtime_error("Failed to handshake: " + ec.message());

	beast::http::request<beast::http::string_body> req(method, target, 11);
	req.set(beast::http::field::host, "localhost");
	if (!body.empty()) {
		req.set(beast::http::field::content_type, "application/json");
		req.body() = body;
		req.prepare_payload();
	}
	beast::http::write(stream, req);

	boost::beast::flat_buffer buffer;
	beast::http::response<beast::http::dynamic_body> res;

	beast::http::read(stream, buffer, res);

	return res;
}
