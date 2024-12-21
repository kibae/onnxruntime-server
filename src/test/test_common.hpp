//
// Created by Kibae Shin on 2023/09/04.
//

#ifndef ONNX_RUNTIME_SERVER_TEST_COMMON_HPP
#define ONNX_RUNTIME_SERVER_TEST_COMMON_HPP

#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

#include "../standalone/model_bin_getter.hpp"

boost::filesystem::path current_file_path(__FILE__);
boost::filesystem::path current_file_dir = current_file_path.parent_path();
boost::filesystem::path root_dir = current_file_dir.parent_path().parent_path().parent_path().parent_path();
boost::filesystem::path test_dir = root_dir / "test";
boost::filesystem::path model_root = test_dir / "fixture";

auto model1_path = model_root / "sample" / "1" / "model.onnx";
auto model2_path = model_root / "sample" / "2" / "model.onnx";

std::string test_model_bin_getter(const std::string &model_name, const std::string &model_version) {
	return onnxruntime_server::get_model_bin(model_root.string(), model_name, model_version);
}

void test_server_run(boost::asio::io_context &io_context, bool *running) {
	auto timeout = std::chrono::milliseconds{500};
	while (*running) {
		io_context.run_for(timeout);
	}
}

#define TIME_MEASURE_INIT auto start_time_point = std::chrono::high_resolution_clock::now();
#define TIME_MEASURE_START start_time_point = std::chrono::high_resolution_clock::now();
#define TIME_MEASURE_STOP                                                                                              \
	{                                                                                                                  \
		auto end_time_point = std::chrono::high_resolution_clock::now();                                               \
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time_point - start_time_point);      \
		std::cout << "Time taken: " << duration.count() << " microseconds" << std::endl;                               \
	}

#endif // ONNX_RUNTIME_SERVER_TEST_COMMON_HPP
