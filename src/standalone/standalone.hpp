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
		void prepare_models(onnxruntime_server::onnx::session_manager &manager);
		void print_config();

		std::string get_model_bin(const std::string &model_name, const std::string &model_version);
	};

} // namespace onnxruntime_server

class SinkCoutWithFilter : public AixLog::SinkCout {
	AixLog::Filter _allow;
	AixLog::Filter _deny;

  public:
	SinkCoutWithFilter(
		const AixLog::Filter &allow, const AixLog::Filter &deny,
		const std::string &format = "%Y-%m-%d %H:%M:%S.#ms [#severity] (#tag_func)"
	)
		: SinkCout(AixLog::Filter(), format), _allow(allow), _deny(deny) {
	}

	void log(const AixLog::Metadata &metadata, const std::string &message) override {
		if ((_allow.is_empty() || _allow.match(metadata)) && (_deny.is_empty() || !_deny.match(metadata)))
			SinkCout::log(metadata, message);
	}
};

class SinkFileWithFilter : public AixLog::SinkFile {
	AixLog::Filter _allow;
	AixLog::Filter _deny;

  public:
	SinkFileWithFilter(
		const AixLog::Filter &allow, const AixLog::Filter &deny, const std::string &filename,
		const std::string &format = "%Y-%m-%d %H:%M:%S.#ms [#severity] (#tag_func)"
	)
		: SinkFile(AixLog::Filter(), filename, format), _allow(allow), _deny(deny) {
	}

	void log(const AixLog::Metadata &metadata, const std::string &message) override {
		if ((_allow.is_empty() || _allow.match(metadata)) && (_deny.is_empty() || !_deny.match(metadata)))
			SinkFile::log(metadata, message);
	}
};

#endif // ONNX_RUNTIME_SERVER_STANDALONE_HPP
