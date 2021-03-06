#include "echoHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;

EchoHandler::EchoHandler(const std::string& p, const NginxConfig& config)
    : url_prefix(p) {
	name = "Echo";
	set_keep_alive_from_config(config);
}

http::response<http::string_body> EchoHandler::handle_request(const http::request<http::string_body>& request) {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::ok);
	res.set(http::field::content_type, "text/plain");
	res.set(http::field::server, "koko.cs130.org");
	res.body() = to_string(request);
	res.prepare_payload();
	return res;
}

std::string EchoHandler::get_url_prefix() {
	return url_prefix;
}