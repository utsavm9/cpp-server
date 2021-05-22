#include "healthHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "handler.h"
#include "server.h"

namespace http = boost::beast::http;

HealthHandler::HealthHandler(__attribute__((unused)) const std::string& url_prefix, __attribute__((unused)) const NginxConfig& config) {
	name = "Health";
}

http::response<http::string_body> HealthHandler::handle_request(__attribute__((unused)) const http::request<http::string_body>& request) {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::ok);
	res.set(http::field::content_type, "text/plain");
	res.set(http::field::server, "koko.cs130.org");
	res.body() = "OK";
	res.prepare_payload();
	return res;
}