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
		po_desc.add_options()("help,h", "Produce help message\n");
		// env: ONNX_WORKERS
		po_desc.add_options()(
			"workers", po::value<int>()->default_value(4),
			"env: ONNX_SERVER_WORKERS\nWorker thread pool size.\nDefault: 4"
		);
		po_desc.add_options()(
			"model-dir", po::value<std::string>()->default_value("models"),
			"env: ONNX_SERVER_MODEL_DIR\nModel directory path.\nThe onnx model files must be located in the "
			"following "
			"path:\n"
			"\"${model_dir}/${model_name}/${model_version}/model.onnx\"\nDefault: ./models"
		);
		po_desc.add_options()(
			"prepare-model", po::value<std::string>(),
			"env: ONNX_SERVER_PREPARE_MODEL\nPre-create some model sessions at server startup.\n\n"
			"Format as a space-separated list of \"model_name:model_version\" or "
			"\"model_name:model_version(session_options, ...)\".\n"
			"\n"
			"Available session_options are\n"
			"  - cuda=device_id[ or true or false]\n"
			"\n"
			"eg) \"model1:v1 model2:v9\"\n    \"model1:v1(cuda=true) model2:v9(cuda=0) model2:v13(cuda=1)\""
		);

		po::options_description po_tcp("TCP Backend");
		po_tcp.add_options()(
			"tcp-port", po::value<short>(),
			"env: ONNX_SERVER_TCP_PORT\nEnable TCP backend and which port number to use."
		);
		po_desc.add(po_tcp);

		po::options_description po_http("HTTP Backend");
		po_http.add_options()(
			"http-port", po::value<short>(),
			"env: ONNX_SERVER_HTTP_PORT\nEnable HTTP backend and which port number to use."
		);
		po_desc.add(po_http);

		po::options_description po_https("HTTPS Backend");
		po_https.add_options()(
			"https-port", po::value<short>(),
			"env: ONNX_SERVER_HTTPS_PORT\nEnable HTTPS backend and which port number to use."
		);
		po_https.add_options()(
			"https-cert", po::value<std::string>(), "env: ONNX_SERVER_HTTPS_CERT\nSSL Certification file path for HTTPS"
		);
		po_https.add_options()(
			"https-key", po::value<std::string>(), "env: ONNX_SERVER_HTTPS_KEY\nSSL Private key file path for HTTPS"
		);
		po_desc.add(po_https);

		po::options_description po_doc("Document");
		po_doc.add_options()(
			"swagger-url-path", po::value<std::string>(),
			"env: ONNX_SERVER_SWAGGER_URL_PATH\nEnable Swagger API document for HTTP/HTTPS backend.\nThis value cannot "
			"start with \"/api/\" and \"/health\" \nIf not specified, swagger document not provided.\neg) /swagger or "
			"/api-docs"
		);
		po_desc.add(po_doc);

		po::options_description po_log("Logging");
		po_log.add_options()(
			"log-level", po::value<std::string>()->default_value("info"),
			"env: ONNX_SERVER_LOG_LEVEL\nLog level(debug, info, warn, error, fatal).\nDefault: info"
		);
		po_log.add_options()(
			"log-file", po::value<std::string>()->default_value(""),
			"env: ONNX_SERVER_LOG_FILE\nLog file path.\nIf not specified, logs will be printed to stdout."
		);
		po_log.add_options()(
			"access-log-file", po::value<std::string>()->default_value(""),
			"env: ONNX_SERVER_ACCESS_LOG_FILE\nAccess log file path.\nIf not specified, logs will be printed to stdout."
		);
		po_desc.add(po_log);

		// env: ONNX_SERVER_*
		auto name_mapper = [&po_desc](const std::string &name) -> std::string {
			if (name.length() <= 12 || name.substr(0, 12) != "ONNX_SERVER_")
				return "";
			auto slice = name.substr(12);
			std::transform(slice.begin(), slice.end(), slice.begin(), [](unsigned char c) { return std::tolower(c); });
			std::replace(slice.begin(), slice.end(), '_', '-');

			if (po_desc.find_nothrow(slice, false) == nullptr)
				return "";
			return slice;
		};

		po::variables_map vm;
		auto config_prio_env = std::getenv("ONNX_SERVER_CONFIG_PRIORITY");
		std::string config_prio(config_prio_env ? config_prio_env : "");
		if (config_prio.length() > 3 && config_prio.substr(0, 3) == "env") {
			// env > cmd
			po::store(po::parse_command_line(argc, argv, po_desc), vm);
			po::store(po::parse_environment(po_desc, name_mapper), vm);
		} else {
			// cmd > env
			po::store(po::parse_environment(po_desc, name_mapper), vm);
			po::store(po::parse_command_line(argc, argv, po_desc), vm);
		}
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << po_desc << "\n";
			return 1;
		}

		config.log_level = vm["log-level"].as<std::string>();
		config.log_file = vm["log-file"].as<std::string>();
		config.access_log_file = vm["access-log-file"].as<std::string>();

		std::map<AixLog::Severity, std::string> log_level_map = {
			{AixLog::Severity::debug, "debug"}, {AixLog::Severity::info, "info"},	{AixLog::Severity::warning, "warn"},
			{AixLog::Severity::error, "error"}, {AixLog::Severity::fatal, "fatal"},
		};
		AixLog::Severity log_level = AixLog::Severity::info;
		for (auto &level : log_level_map) {
			if (config.log_level == level.second) {
				log_level = level.first;
				break;
			}
		}

		std::shared_ptr<AixLog::Sink> log_file;
		std::shared_ptr<AixLog::Sink> log_access_file;
		AixLog::Filter for_access;
		for_access.add_filter("ACCESS", AixLog::Severity::info);

		if (config.log_file.empty())
			log_file = std::make_shared<SinkCoutWithFilter>(log_level, for_access);
		else
			log_file = std::make_shared<SinkFileWithFilter>(log_level, for_access, config.log_file);

		if (config.access_log_file.empty())
			log_access_file = std::make_shared<SinkCoutWithFilter>(for_access, AixLog::Filter());
		else
			log_access_file = std::make_shared<SinkFileWithFilter>(
				for_access, AixLog::Filter(), config.access_log_file, "%Y-%m-%d %H-%M-%S.#ms [#severity]"
			);

		AixLog::Log::init({log_file, log_access_file});

		if (vm.count("workers"))
			config.num_threads = vm["workers"].as<int>();

		if (vm.count("model-dir"))
			config.model_dir = vm["model-dir"].as<std::string>();
		else
			config.model_dir = "models";

		if (vm.count("prepare-model"))
			config.prepare_model = vm["prepare-model"].as<std::string>();

		if (vm.count("tcp-port")) {
			config.use_tcp = true;
			config.tcp_port = vm["tcp-port"].as<short>();
		}

		if (vm.count("http-port")) {
			config.use_http = true;
			config.http_port = vm["http-port"].as<short>();
		}

		if (vm.count("https-port")) {
			config.use_https = true;
			config.https_port = vm["https-port"].as<short>();
		}

		if (vm.count("https-cert"))
			config.https_cert = vm["https-cert"].as<std::string>();

		if (vm.count("https-key"))
			config.https_key = vm["https-key"].as<std::string>();

		if (config.use_https) {
			if (config.https_cert.empty())
				throw std::runtime_error("SSL Certification file path is not specified.");
			if (config.https_key.empty())
				throw std::runtime_error("SSL Private key file path is not specified.");
		}

		if (vm.count("swagger-url-path")) {
			config.swagger_url_path = vm["swagger-url-path"].as<std::string>();
			// cannot start with "/api/" and "/health"
			if ((config.swagger_url_path.length() >= 5 && config.swagger_url_path.substr(0, 5) == "/api/") ||
				(config.swagger_url_path.length() >= 7 && config.swagger_url_path.substr(0, 7) == "/health"))
				throw std::runtime_error(R"(Swagger URL path cannot start with "/api" and "/health")");
		}

		model_root = config.model_dir;

		print_config();

		if (!exists(model_root))
			throw std::runtime_error("Model directory does not exist: " + model_root.string());

		if (!config.use_tcp && !config.use_http
#ifdef HAS_OPENSSL
			&& !config.use_https
#endif
		) {
			std::cout << po_desc << "\n\n";
			throw std::runtime_error("No backend(TCP, HTTP, HTTPS) is enabled");
		}
	} catch (boost::program_options::error_with_option_name &e) {
		std::cerr << "Config process error:\n" << e.get_option_name() << e.what() << std::endl;
		return 1;
	} catch (std::exception &e) {
		std::cerr << "Config process error:\n" << e.what() << std::endl;
		return 1;
	}

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

void onnxruntime_server::standalone::prepare_models(onnxruntime_server::onnx::session_manager &manager) {
	auto model_keys = Orts::onnx::session_key_with_option::parse(config.prepare_model);
	if (model_keys.empty())
		return;

	for (auto &key : model_keys) {
		manager.create_session(key.model_name, key.model_version, key.option);
	}
}

void onnxruntime_server::standalone::print_config() {
	// print config values
	auto config_json = ordered_json::object();
	config_json["workers"] = config.num_threads;
	config_json["model_dir"] = config.model_dir;

	config_json["tcp"] = json::object();
	config_json["tcp"]["use"] = config.use_tcp;
	if (config.use_tcp)
		config_json["tcp"]["port"] = config.tcp_port;

	config_json["http"] = json::object();
	config_json["http"]["use"] = config.use_http;
	if (config.use_http)
		config_json["http"]["port"] = config.http_port;

	config_json["https"] = json::object();
	config_json["https"]["use"] = config.use_https;
	if (config.use_https) {
		config_json["https"]["port"] = config.https_port;
		config_json["https"]["cert"] = config.https_cert;
		config_json["https"]["key"] = config.https_key;
	}

	if (config.use_http || config.use_https)
		config_json["swagger_url_path"] = config.swagger_url_path;

	config_json["log"] = json::object();
	config_json["log"]["level"] = config.log_level;
	config_json["log"]["file"] = config.log_file;
	config_json["log"]["access_file"] = config.access_log_file;

	PLOG(L_INFO) << "Config values:\n" << config_json.dump(2) << std::endl;
}
