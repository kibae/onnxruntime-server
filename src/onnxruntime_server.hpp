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
#include "utils/half_fp.hpp"
#include "utils/json.hpp"

#include <onnxruntime_cxx_api.h>

using asio = boost::asio::ip::tcp;
using json = nlohmann::json;

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
		class value_info {
		  public:
			const std::string name;
			const ONNXTensorElementDataType element_type;
			const std::vector<int64_t> shape;

			value_info(std::string name, ONNXTensorElementDataType element_type, std::vector<int64_t> shape);

			[[nodiscard]] std::string type_name() const;
			[[nodiscard]] std::string type_to_string() const;
			static const char *type_name(ONNXTensorElementDataType element_type);

			json::array_t get_tensor_data(Ort::Value &tensors) const;
		};

		class session_key {
		  public:
			std::string model_name;
			std::string model_version = 0;

			session_key(std::string model_name, std::string model_version);

			bool operator<(const session_key &other) const;
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

			explicit session(session_key key, const json &option);

		  public:
			session_key key;
			session(session_key key, const std::string &path);
			session(session_key key, const std::string &path, const json &option);
			session(session_key key, const void *model_data, size_t modal_data_length, const json &option);
			~session();

			std::vector<Ort::Value>
			run(const Ort::MemoryInfo &memory_info, const std::vector<Ort::Value> &input_values);

			void touch();
			json to_json() const;

			[[nodiscard]] const std::vector<value_info> &inputs() const;
			[[nodiscard]] const std::vector<value_info> &outputs() const;
		};

		class session_manager {
		  private:
			std::recursive_mutex mutex;
			std::map<session_key, session *> sessions;
			model_bin_getter_t model_bin_getter;

		  public:
			explicit session_manager(model_bin_getter_t model_bin_getter);
			~session_manager();

			std::map<session_key, session *> &get_sessions() {
				return sessions;
			}

			session *get_session(const std::string &model_name, const std::string &model_version);
			session *get_session(const session_key &key);
			session *
			create_session(const std::string &model_name, const std::string &model_version, const json &option);
			session *create_session(
				const std::string &model_name, const std::string &model_version, const std::string &bin,
				const json &option
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
				onnxruntime_server::onnx::session *session;
				std::map<std::string, input_value *> inputs;

			  public:
				context(class session *session, const json &json_str);
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
			onnx::session_manager *onnx_session_manager;
			json raw;

		  public:
			std::string model_name;
			std::string model_version;
			json data;

			session_task(onnx::session_manager *onnx_session_manager, const std::string &buf);
			session_task(
				onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version,
				json data
			);
			session_task(
				onnx::session_manager *onnx_session_manager, const std::string &model_name,
				const std::string &model_version
			);
		};

		class create_session : public session_task {
		  public:
			json option;

			explicit create_session(onnx::session_manager *onnx_session_manager, const std::string &buf);
			explicit create_session(
				onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version,
				json data, json option
			);

			std::string name() override;

			json run() override;
		};

		class execute_session : public session_task {
		  public:
			std::string name() override;

			explicit execute_session(onnx::session_manager *onnx_session_manager, const std::string &buf);
			explicit execute_session(
				onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version,
				json data
			);
			json run() override;
		};

		class get_session : public session_task {
		  public:
			std::string name() override;

			explicit get_session(onnx::session_manager *onnx_session_manager, const std::string &buf);
			explicit get_session(
				onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version
			);
			json run() override;
		};

		class list_session : public task {
			onnx::session_manager *onnx_session_manager;

		  public:
			std::string name() override;

			explicit list_session(onnx::session_manager *onnx_session_manager);
			json run() override;
		};

		class destroy_session : public session_task {
		  public:
			std::string name() override;

			explicit destroy_session(onnx::session_manager *onnx_session_manager, const std::string &buf);
			explicit destroy_session(
				onnx::session_manager *onnx_session_manager, std::string model_name, std::string model_version
			);
			json run() override;
		};

		std::shared_ptr<task> create(onnx::session_manager *onnx_session_manager, int16_t type, const std::string &buf);

	} // namespace task

	class config {
	  public:
		bool use_tcp = true;
		short tcp_port = 0;

		bool use_http = true;
		short http_port = 80;

		bool use_https = false;
		short https_port = 443;
		std::string https_cert;
		std::string https_key;

		std::string log_level;
		std::string log_file;
		std::string access_log_file;

		long num_threads = 4;
		std::string model_dir;
		model_bin_getter_t model_bin_getter{};
	};

	namespace transport {
		class server {
			void accept();

		  protected:
			boost::asio::io_context &io_context;
			asio::socket socket;
			asio::acceptor acceptor;
			uint_least16_t assigned_port = 0;

			onnx::session_manager *onnx_session_manager;

			builtin_thread_pool *worker_pool;
			virtual void client_connected(asio::socket socket) = 0;

		  public:
			server(
				boost::asio::io_context &io_context, onnx::session_manager *onnx_session_manager,
				builtin_thread_pool *worker_pool, int port
			);
			~server();

			builtin_thread_pool *get_worker_pool();
			onnx::session_manager *get_onnx_session_manager();
			[[nodiscard]] uint_least16_t port() const;
		};

	} // namespace transport

} // namespace onnxruntime_server

namespace Orts = onnxruntime_server;

#endif // ONNX_RUNTIME_SERVER_HPP
