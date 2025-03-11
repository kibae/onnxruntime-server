//
// Created by Kibae Shin on 2023/08/30.
//

#ifndef ONNX_RUNTIME_SERVER_HPP
#define ONNX_RUNTIME_SERVER_HPP

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <list>
#include <queue>
#include <string>
#include <thread>
#include <utility>

#include "thread_pool.hpp"
#include "utils/aixlog.hpp"
#include "utils/exceptions.hpp"
#include "utils/json.hpp"
#include "utils/windows.h"

#include <onnxruntime_cxx_api.h>

using asio = boost::asio::ip::tcp;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

/**
 * onnxruntime_server is a namespace that provides a server that can execute ONNX models.
 * The server is implemented using Boost.Asio and can be used for both TCP(tcp/tcp_server.hpp) and
 * HTTP(http/http_server.hpp).
 *
 * namespace shorthand: Orts
 */
namespace onnxruntime_server {
	typedef std::function<std::string(const std::string &, const std::string &)> model_bin_getter_t;

	namespace onnx {
		std::string version();

		class value_info {
		  public:
			const std::string name;
			const ONNXTensorElementDataType element_type;
			std::vector<int64_t> shape;

			value_info(std::string name, ONNXTensorElementDataType element_type, std::vector<int64_t> shape);

			[[nodiscard]] std::string type_name() const;
			[[nodiscard]] std::string type_to_string() const;
			static const char *type_name(ONNXTensorElementDataType element_type);

			json::array_t get_tensor_data(Ort::Value &tensors) const;
		};

		class session_key {
		  public:
			std::string model_name;
			std::string model_version;

			session_key(std::string model_name, std::string model_version);
			bool operator<(const session_key &other) const;

			static bool is_valid_model_name(const std::string &model_key);
			static bool is_valid_model_version(const std::string &model_version);
		};

		class session_key_with_option : public session_key {
		  public:
			json option;

			session_key_with_option(std::string model_name, std::string model_version, json option)
				: session_key(model_name, model_version), option(option) {
			}

			static std::vector<session_key_with_option> parse(const std::string &model_key_list);
		};

		class session {
		  private:
			Ort::Env env;
			Ort::SessionOptions session_options;
			Ort::Session *ort_session{};
			std::chrono::system_clock::time_point created_at;
			std::chrono::system_clock::time_point last_executed_at;
			long execution_count = 0;

			Ort::AllocatorWithDefaultOptions allocator;
			std::vector<value_info> _inputs;
			std::vector<value_info> _outputs;

			size_t inputCount = 0;
			std::vector<std::string> _inputNames;
			std::vector<const char *> inputNames;

			size_t outputCount = 0;
			std::vector<std::string> _outputNames;
			std::vector<const char *> outputNames;

			json _option = json::object();

			void init();
			void override_shape(const char *option_key, value_info &value);

			explicit session(session_key key, const json &option);

		  public:
			session_key key;
			explicit session(session_key key, const std::string &path, const json &option = json::object());
			explicit session(
				session_key key, const char *model_data, size_t model_data_length, const json &option = json::object()
			);
			~session();

			std::vector<Ort::Value>
			run(const Ort::MemoryInfo &memory_info, const std::vector<Ort::Value> &input_values);

			void touch();
			json to_json() const;

			[[nodiscard]] const std::vector<value_info> &inputs() const;
			[[nodiscard]] const std::vector<value_info> &outputs() const;
		};

		typedef std::shared_ptr<session> session_ptr;

		class session_manager {
		  private:
			std::recursive_mutex mutex;
			std::map<session_key, session_ptr> sessions;
			model_bin_getter_t model_bin_getter;

		  public:
			explicit session_manager(const model_bin_getter_t &model_bin_getter, long num_threads);
			~session_manager();

			builtin_thread_pool thread_pool;

			const std::map<session_key, session_ptr> &get_sessions() {
				return sessions;
			}

			session_ptr get_session(const std::string &model_name, const std::string &model_version);
			session_ptr get_session(const session_key &key);
			session_ptr create_session(
				const std::string &model_name, const std::string &model_version, const json &option,
				const char *model_data = nullptr, size_t model_data_length = 0
			);
			void remove_session(const std::string &model_name, const std::string &model_version);
			void remove_session(const session_key &key);
		};

		namespace execution {
			class input_value {
				std::vector<std::function<void(void)>> deallocators;

			  public:
				Ort::Value tensors = Ort::Value(nullptr);
				input_value(
					const Ort::MemoryInfo &memory_info, const value_info &info, const json::value_type &json_value
				);
				~input_value();

				std::vector<int64_t> batched_shape(const std::vector<int64_t> &shape, size_t value_count);
			};

			class context {
			  private:
				Ort::MemoryInfo memory_info;
				std::shared_ptr<onnxruntime_server::onnx::session> session;
				std::map<std::string, input_value *> inputs;

			  public:
				context(std::shared_ptr<class session> session, const json &json_str);
				~context();

				void flat_json_values(const json::value_type &data, std::vector<json::value_type> *json_values);
				std::vector<Ort::Value> run();
				json tensors_to_json(std::vector<Ort::Value> &tensors);
			};
		} // namespace execution

	} // namespace onnx

	namespace task {
		enum type : int16_t {
			CREATE_SESSION = 1,
			EXECUTE_SESSION = 5,
			DESTROY_SESSION = 9,
			LIST_SESSION = 21,
			GET_SESSION = 22,
		};

		class benchmark {
		  private:
			std::chrono::time_point<std::chrono::high_resolution_clock> time;

		  public:
			void touch() {
				time = std::chrono::high_resolution_clock::now();
			}

			long long get_duration() {
				return std::chrono::duration_cast<std::chrono::microseconds>(
						   std::chrono::high_resolution_clock::now() - time
				)
					.count();
			}
		};

		// abstract
		class task {
		  public:
			virtual std::string name() = 0;
			virtual json run() = 0;
		};

		class session_task : public task {
		  protected:
			onnx::session_manager &onnx_session_manager;

		  public:
			std::string model_name;
			std::string model_version;

			explicit session_task(onnx::session_manager &onnx_session_manager, const json &request_json);
			explicit session_task(
				onnx::session_manager &onnx_session_manager, std::string model_name, std::string model_version
			);
		};

		class create_session : public session_task {
		  public:
			json option;
			const char *model_data = nullptr;
			size_t model_data_length = 0;

			explicit create_session(
				onnx::session_manager &onnx_session_manager, const json &request_json, const char *model_data = nullptr,
				size_t model_data_length = 0
			);
			explicit create_session(
				onnx::session_manager &onnx_session_manager, const std::string &model_name,
				const std::string &model_version, json option, const char *model_data = nullptr,
				size_t model_data_length = 0
			);
			std::string name() override;
			json run() override;
		};

		class execute_session : public session_task {
		  public:
			json data;

			explicit execute_session(onnx::session_manager &onnx_session_manager, const json &request_json);
			explicit execute_session(
				onnx::session_manager &onnx_session_manager, const std::string &model_name,
				const std::string &model_version, json data
			);
			std::string name() override;
			json run() override;
		};

		class get_session : public session_task {
		  public:
			explicit get_session(onnx::session_manager &onnx_session_manager, const json &request_json);
			explicit get_session(
				onnx::session_manager &onnx_session_manager, const std::string &model_name,
				const std::string &model_version
			);
			std::string name() override;
			json run() override;
		};

		class list_session : public task {
			onnx::session_manager &onnx_session_manager;

		  public:
			explicit list_session(onnx::session_manager &onnx_session_manager);
			std::string name() override;
			json run() override;
		};

		class destroy_session : public session_task {
		  public:
			explicit destroy_session(onnx::session_manager &onnx_session_manager, const json &request_json);
			explicit destroy_session(
				onnx::session_manager &onnx_session_manager, const std::string &model_name,
				const std::string &model_version
			);
			std::string name() override;
			json run() override;
		};

	} // namespace task

	class config {
	  public:
		bool use_tcp = false;
		short tcp_port = 0;

		bool use_http = false;
		short http_port = 80;

		bool use_https = false;
		short https_port = 443;
		std::string https_cert;
		std::string https_key;
		std::string swagger_url_path;

		std::string log_level;
		std::string log_file;
		std::string access_log_file;

		long num_threads = 4;
		std::string model_dir;
		std::string prepare_model;
		model_bin_getter_t model_bin_getter{};
		long request_payload_limit = 1024 * 1024 * 10;
	};

	namespace transport {
		class server {
			void accept();

		  protected:
			boost::asio::io_context &io_context;
			asio::socket socket;
			asio::acceptor acceptor;
			uint_least16_t assigned_port = 0;
			long request_payload_limit_;

			onnx::session_manager &onnx_session_manager;

			virtual void client_connected(asio::socket socket) = 0;

		  public:
			server(
				boost::asio::io_context &io_context, onnx::session_manager &onnx_session_manager, int port,
				long request_payload_limit
			);
			~server();

			onnx::session_manager &get_onnx_session_manager();
			[[nodiscard]] long request_payload_limit() const;
			[[nodiscard]] uint_least16_t port() const;
		};

	} // namespace transport

} // namespace onnxruntime_server

namespace Orts = onnxruntime_server;

// consts
#define NUM_MAX(a, b) (a > b ? a : b)
#define NUM_MIN(a, b) (a < b ? a : b)

#endif // ONNX_RUNTIME_SERVER_HPP
