//
// Created by kibae on 23. 8. 17.
//

#include "../../transport/tcp/tcp_server.hpp"
#include "../test_common.hpp"

json tcp_request(
	uint_least16_t port, int16_t type, const json &json, unsigned char *post = nullptr, size_t post_size = 0
) {
	boost::asio::io_context io_context;
	boost::asio::ip::tcp::socket socket(io_context);
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), port);
	socket.connect(endpoint);

	auto json_data = json.dump();
	struct onnxruntime_server::transport::tcp::protocol_header header = {};
	header.type = htons(type);
	header.length = HTONLL(json_data.size() + post_size);
	header.json_length = HTONLL(json_data.size());
	header.post_length = HTONLL(post_size);

	std::vector<boost::asio::const_buffer> buffers;
	buffers.emplace_back(&header, sizeof(header));
	buffers.emplace_back(json_data.c_str(), json_data.size());
	if (post != nullptr)
		buffers.emplace_back(post, post_size);

	boost::asio::write(socket, buffers);

	Orts::transport::tcp::protocol_header res_header = {0, 0, 0, 0};

	std::size_t length = socket.read_some(boost::asio::buffer(&res_header, sizeof(Orts::transport::tcp::protocol_header)));
	if (length < sizeof(Orts::transport::tcp::protocol_header))
		return json::object();

	res_header.type = ntohs(res_header.type);
	res_header.length = NTOHLL(res_header.length);
	res_header.json_length = NTOHLL(res_header.json_length);
	res_header.post_length = NTOHLL(res_header.post_length);

	std::string chunk;
	chunk.resize(MAX_RECV_BUF_LENGTH);
	std::string buffer;
	while (buffer.size() < res_header.length) {
		length = socket.read_some(
			boost::asio::buffer(chunk.data(), NUM_MIN(MAX_RECV_BUF_LENGTH, header.length - buffer.length())));
		if (length <= 0)
			return json::object();

		buffer.append(chunk.data(), length);
	}

	return json::parse(buffer);
}

TEST(test_onnxruntime_server_tcp, TcpServerTest) {
	Orts::config config;
	config.model_bin_getter = test_model_bin_getter;

	boost::asio::io_context io_context;
	Orts::onnx::session_manager manager(config.model_bin_getter, config.num_threads);
	Orts::transport::tcp::tcp_server server(io_context, config, manager);
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

	{ // API: Create session2
		json body = json::parse(R"({"model":"sample","version":"path"})");
		body["option"]["path"] = model2_path.string();
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::CREATE_SESSION, body);
		TIME_MEASURE_STOP
		std::cout << "API: Create session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], "path");
	}

	{ // API: Get session
		json body = json::parse(R"({"model":"sample","version":"path"})");
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::GET_SESSION, body);
		TIME_MEASURE_STOP
		std::cout << "API: Get session\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json["model"], "sample");
		ASSERT_EQ(res_json["version"], "path");
	}

	{ // API: List session
		TIME_MEASURE_START
		auto res_json = tcp_request(server.port(), Orts::task::type::LIST_SESSION, "");
		TIME_MEASURE_STOP
		std::cout << "API: List sessions\n" << res_json.dump(2) << "\n";
		ASSERT_EQ(res_json.size(), 2);
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
		ASSERT_EQ(res_json.size(), 1);
	}

	running = false;
	server_thread.join();
}
