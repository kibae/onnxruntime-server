//
// Created by Kibae Shin on 2023/09/04.
//

#ifndef ONNX_RUNTIME_SERVER_TEST_COMMON_HPP
#define ONNX_RUNTIME_SERVER_TEST_COMMON_HPP

#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

boost::filesystem::path current_file_path(__FILE__);
boost::filesystem::path current_file_dir = current_file_path.parent_path();
boost::filesystem::path root_dir = current_file_dir.parent_path().parent_path();
boost::filesystem::path test_dir = root_dir / "test";

auto model1_path = test_dir / "fixture" / "sample-model1.onnx";
// https://huggingface.co/alimazhar-110/website_classification
auto model2_path = test_dir / "fixture" / "website_classification.onnx";

std::string test_model_bin_getter(const std::string &model_name, int model_version) {
	std::string model_path;
	switch (model_version) {
	case 1:
		model_path = model1_path.string();
		break;
	case 2:
		model_path = model2_path.string();
		break;
	}

	std::ifstream file(model_path, std::ios::binary);
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
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
