//
// Created by kibae on 23. 8. 17.
//

#include "../transport/tcp/tcp_server.hpp"
#include "test_common.hpp"

json tcp_request(short port, int16_t type, json body) {
	boost::asio::io_context io_context;
	boost::asio::ip::tcp::socket socket(io_context);
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
	socket.connect(endpoint);

	auto data = body.dump();
	struct onnxruntime_server::transport::tcp::protocol_header header = {};
	header.type = htons(type);
	header.length = htonl(data.size());

	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(boost::asio::buffer(&header, sizeof(header)));
	buffers.push_back(boost::asio::buffer(data));

	boost::asio::write(socket, buffers);

	struct Orts::transport::tcp::protocol_header res_header = {};
	socket.read_some(boost::asio::buffer(&res_header, sizeof(res_header)));
	res_header.type = ntohs(res_header.type);
	res_header.length = ntohl(res_header.length);

	std::string res_data;
	while (res_data.length() < res_header.length) {
		char buffer[1024 * 4];
		std::size_t bytes_read = socket.read_some(boost::asio::buffer(buffer));
		res_data.append(buffer, bytes_read);
	}
	socket.close();

	return json::parse(res_data);
}

TEST(test_onnxruntime_server_tcp, TcpServerTest) {
	Orts::config config;
	config.model_bin_getter = test_model_bin_getter;

	boost::asio::io_context io_context;
	Orts::onnx::session_manager manager(config.model_bin_getter);
	Orts::builtin_thread_pool worker_pool(config.num_threads);
	Orts::transport::tcp::tcp_server server(io_context, config, &manager, &worker_pool);
	ASSERT_GT(server.port(), 0);

	bool running = true;
	std::thread server_thread([&io_context, &running]() { test_server_run(io_context, &running); });

	TIME_MEASURE_INIT

	{ // API: Raise error
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), -1, "");
		TIME_MEASURE_STOP
		std::cout << "API: Raise error\n" << res_json.dump(2) << "\n";
		ASSERT_TRUE(res_json.contains("error"));
		ASSERT_EQ(res_json["error"], "Invalid task type");
	}

	{ // API: Create session
		json body = json::parse(R"({"model":"sample","version":"1"})");
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::CREATE_SESSION, body);
		TIME_MEASURE_STOP
		std::cout << "API: Create session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], "1");
	}

	{ // API: Get session
		json body = json::parse(R"({"model":"sample","version":"1"})");
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::GET_SESSION, body);
		TIME_MEASURE_STOP
		std::cout << "API: Get session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], "1");
	}

	{ // API: List session
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::LIST_SESSION, "");
		TIME_MEASURE_STOP
		std::cout << "API: List sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json.size(), 1);
		ASSERT_EQ(res_json[0]["model"], "sample");
		ASSERT_EQ(res_json[0]["version"], "1");
	}

	{ // API: Execute session
		auto input = json::parse(R"({"model":"sample","version":"1","data":{"x":[[1]],"y":[[2]],"z":[[3]]}})");
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::EXECUTE_SESSION, input);
		TIME_MEASURE_STOP
		std::cout << "API: Execute sessions\n" << res_json.dump(2) << "\n";
		ASSERT_TRUE(res_json.contains("output"));
		ASSERT_EQ(res_json["output"].size(), 1);
		ASSERT_GT(res_json["output"][0], 0);
	}

	{ // API: Destroy session
		json body = json::parse(R"({"model":"sample","version":"1"})");
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::DESTROY_SESSION, body);
		TIME_MEASURE_STOP
		std::cout << "API: Destroy sessions\n" << res_json.dump(2) << "\n";
		ASSERT_TRUE(res_json);
	}

	{ // API: List session
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::LIST_SESSION, "");
		TIME_MEASURE_STOP
		std::cout << "API: List sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json.size(), 0);
	}

	running = false;
	server_thread.join();
}
