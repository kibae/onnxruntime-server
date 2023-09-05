//
// Created by Kibae Shin on 2023/08/30.
//

#include "standalone.hpp"
#include <fstream>
#include <sstream>

onnxruntime_server::standalone::standalone() : config() {
}

int onnxruntime_server::standalone::init_config(int argc, char **argv) {
	try {

		po::options_description po_desc("ONNX Runtime Server options", 100);
		po_desc.add_options()("help,h", "Produce help message");
		po_desc.add_options()("workers", po::value<int>()->default_value(4), "Worker thread pool size. Default: 4");
		po_desc.add_options()(
			"model-dir", po::value<std::string>()->default_value("models"),
			"Model directory path.\nThe onnx model files must be located in the following path:\n"
			"\"${model_dir}/${model_name}/${model_version}/model.onnx\"\nDefault: ./models"
		);

		po::options_description po_tcp("TCP Backend");
		po_tcp.add_options()("disable-tcp", "Disable TCP backend");
		po_tcp.add_options()(
			"tcp-port", po::value<short>()->default_value(8001),
			"Enable TCP backend and which port number to use. Default: 8001"
		);
		po_desc.add(po_tcp);

		po::options_description po_http("HTTP Backend");
		po_http.add_options()("disable-http", "Disable HTTP backend. Default: false");
		po_http.add_options()(
			"http-port", po::value<short>()->default_value(80),
			"Enable HTTP backend and which port number to use. Default: 80"
		);
		po_desc.add(po_http);

		po::options_description po_https("HTTPS Backend");
		po_https.add_options()("disable-https", "Disable HTTPS backend. Default: true");
		po_https.add_options()("https-port", po::value<short>(), "Enable HTTPS backend and which port number to use");
		po_https.add_options()("https-cert", po::value<std::string>(), "SSL Certification file path for HTTPS");
		po_https.add_options()("https-key", po::value<std::string>(), "SSL Private key file path for HTTPS");
		po_desc.add(po_https);

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, po_desc), vm);
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << po_desc << "\n";
			return 1;
		}

		if (vm.count("workers"))
			config.num_threads = vm["workers"].as<int>();

		if (vm.count("model-dir"))
			config.model_dir = vm["model-dir"].as<std::string>();
		else
			config.model_dir = "models";

		if (vm.count("disable-tcp"))
			config.use_tcp = false;
		else if (vm.count("tcp-port")) {
			config.use_tcp = true;
			config.tcp_port = vm["tcp-port"].as<short>();
		}

		if (vm.count("disable-http"))
			config.use_http = false;
		else if (vm.count("http-port")) {
			config.use_http = true;
			config.http_port = vm["http-port"].as<short>();
		}

		if (vm.count("disable-https"))
			config.use_https = false;
		else if (vm.count("https-port")) {
			config.use_https = true;
			config.https_port = vm["https-port"].as<short>();

			if (vm.count("https-cert"))
				config.https_cert = vm["https-cert"].as<std::string>();
			else
				throw std::runtime_error("SSL Certification file path is not specified");

			if (vm.count("https-key"))
				config.https_cert = vm["https-key"].as<std::string>();
			else
				throw std::runtime_error("SSL Private key file path is not specified");
		}

		model_root = config.model_dir;
		if (!exists(model_root))
			throw std::runtime_error("Model directory does not exist: " + model_root.string());

		if (!config.use_tcp && !config.use_http
#ifdef HAS_OPENSSL
			&& !config.use_https
#endif
		)
			throw std::runtime_error("No backend(TCP, HTTP, HTTPS) is enabled");
	} catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}

	// print config values
	auto config_json = json::object();
	config_json["num_threads"] = config.num_threads;
	config_json["model_dir"] = config.model_dir;

	config_json["tcp"] = json::object();
	config_json["tcp"]["use"] = config.use_tcp;
	config_json["tcp"]["port"] = config.tcp_port;

	config_json["http"] = json::object();
	config_json["http"]["use"] = config.use_http;
	config_json["http"]["port"] = config.http_port;

	config_json["https"] = json::object();
	config_json["https"]["use"] = config.use_https;
	config_json["https"]["port"] = config.https_port;
	config_json["https"]["cert"] = config.https_cert;
	config_json["https"]["key"] = config.https_key;

	std::cout << "* Config values:\n" << config_json.dump(2) << "\n";

	config.model_bin_getter = std::bind(&standalone::get_model_bin, this, std::placeholders::_1, std::placeholders::_2);

	return 0;
}

std::string
onnxruntime_server::standalone::get_model_bin(const std::string &model_name, const std::string &model_version) {
	auto model_path = (model_root / model_name / model_version / "model.onnx").string();
	std::ifstream file(model_path, std::ios::binary);
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}
