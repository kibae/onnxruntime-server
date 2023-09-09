//
// Created by Kibae Shin on 2023/09/02.
//
#include "../../onnxruntime_server.hpp"
#include "../test_common.hpp"

TEST(unit_test_session_key, Parse) {
	// empty cases
	std::string empty_cases[] = {"", " ", "\n", "\r\n", "\n \n", " \r \n \r \n "};
	for (const auto &empty_case : empty_cases) {
		// std::cout << "* empty_case: " << empty_case << "\n";
		ASSERT_EQ(Orts::onnx::session_key_with_option::parse(empty_case).size(), 0);
	}

	// error cases
	std::string fail_cases[] = {
		" model: version", " model :version", "model:ver\bsion\r\n", "model:version(cuda=true",
		"model:version (cuda=1)"};
	for (const auto &fail_case : fail_cases) {
		// std::cout << "* fail_case: " << fail_case << "\n";
		ASSERT_THROW(Orts::onnx::session_key_with_option::parse(fail_case), std::runtime_error);
	}

	// success cases
	std::unordered_map<std::string, size_t> success_cases = {
		{"model:version \bmodel:version", 2},
		{"model:version(cuda =\n1, cuda =2)", 1},
		{"model:version(cuda =\n1)\tmodel:version", 2},
		{"model:version model:version model:version(cuda= 1)", 3},
		{"  model:version", 1},
		{" model:version  model:version  ", 2},
		{"   model:version   model:version(cuda =\r\n1, cuda =\r\n2) model:version ", 3},
		{"model:version ", 1},
		{" model:version \n model:version", 2},
		{" \r \n model:version \n model:version", 2},
		{"   model:version \r \n  model:version model:version ", 3},
	};
	for (const auto &success_case : success_cases) {
		// std::cout << "* success_case: " << success_case.first << "\n";
		ASSERT_EQ(Orts::onnx::session_key_with_option::parse(success_case.first).size(), success_case.second);
	}

	auto parse_case1 = Orts::onnx::session_key_with_option::parse("model:version");
	ASSERT_FALSE(parse_case1[0].option.contains("cuda"));

	auto parse_case2 = Orts::onnx::session_key_with_option::parse("model:version(cuda=1)");
	ASSERT_EQ(parse_case2[0].option["cuda"], 1);

	auto parse_case3 = Orts::onnx::session_key_with_option::parse("model:version(cuda=1, cuda=2)");
	ASSERT_EQ(parse_case3[0].option["cuda"], 2);

	auto parse_case4 = Orts::onnx::session_key_with_option::parse("model:version(cuda=true)");
	ASSERT_TRUE(parse_case4[0].option["cuda"]);
}
