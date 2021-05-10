#include "echoService.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;

EchoService::EchoService(const std::string& prefix)
    : url_prefix(prefix) {
}

EchoService::EchoService(const std::string& p, __attribute__((unused)) const NginxConfig& config)
    : url_prefix(p) {
}

http::response<http::string_body> EchoService::handle_request(const http::request<http::string_body>& request) {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::ok);
	res.set(http::field::content_type, "text/plain");
	res.set(http::field::server, "koko.cs130.org");
	res.body() = to_string(request);
	res.prepare_payload();
	return res;
}

std::string EchoService::get_url_prefix() {
	return this->url_prefix;
}