//
// Created by Kibae Shin on 2023/08/30.
//

#ifndef ONNX_RUNTIME_SERVER_STANDALONE_HPP
#define ONNX_RUNTIME_SERVER_STANDALONE_HPP

#include "../onnxruntime_server.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace onnxruntime_server {
	class standalone {
	  private:
		boost::filesystem::path model_root;

	  public:
		onnxruntime_server::config config;

		standalone();

		int init_config(int argc, char *argv[]);
		std::string get_model_bin(const std::string &model_name, const std::string &model_version);
	};

} // namespace onnxruntime_server

#endif // ONNX_RUNTIME_SERVER_STANDALONE_HPP
