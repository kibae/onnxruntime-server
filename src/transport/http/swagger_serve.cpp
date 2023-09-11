//
// Created by Kibae Shin on 2023/09/08.
//

#include "http_server.hpp"

#include "swagger/document_bin_data.h"

onnxruntime_server::transport::http::swagger_serve::swagger_serve(const std::string &swagger_url_path)
	: swagger_url_path(swagger_url_path) {
}

bool onnxruntime_server::transport::http::swagger_serve::is_swagger_url(const std::string &url) const {
	return !swagger_url_path.empty() && swagger_url_path.length() <= url.length() && url.find(swagger_url_path) == 0;
}

std::shared_ptr<beast::http::response<beast::http::string_body>>
onnxruntime_server::transport::http::swagger_serve::get_response(const std::string &url, unsigned http_version) {
	std::string content_type = "text/plain";
	std::shared_ptr<std::string> body = std::make_shared<std::string>("Not Found");

	auto res =
		std::make_shared<beast::http::response<beast::http::string_body>>(beast::http::status::not_found, http_version);

	if (url.rfind("/openapi.yaml") == url.length() - 13) {
		if (_cache_openapi_yaml == nullptr || _cache_openapi_yaml->empty())
			_cache_openapi_yaml = std::make_shared<std::string>((char *)swagger_openapi_yaml, swagger_openapi_yaml_len);
		res->result(beast::http::status::ok);
		content_type = "text/yaml";
		body = _cache_openapi_yaml;
	} else if (url == swagger_url_path || url == swagger_url_path + "/") {
		if (url.back() != '/') {
			res->result(beast::http::status::found);
			res->set(beast::http::field::location, url + "/");
		} else {
			if (_cache_index_html == nullptr || _cache_index_html->empty())
				_cache_index_html = std::make_shared<std::string>((char *)swagger_index_html, swagger_index_html_len);
			res->result(beast::http::status::ok);
			content_type = "text/html";
			body = _cache_index_html;
		}
	}

	res->set(beast::http::field::content_type, content_type);
	res->body() = body->data();
	res->prepare_payload();
	return res;
}
