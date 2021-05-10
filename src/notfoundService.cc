#include "notfoundService.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;

NotFoundService::NotFoundService(const std::string& prefix)
    : url_prefix(prefix) {
}

NotFoundService::NotFoundService(const std::string& p, __attribute__((unused)) const NginxConfig& config)
    : url_prefix(p) {
}

http::response<http::string_body> NotFoundService::handle_request(__attribute__((unused)) const http::request<http::string_body>& request) {
	return not_found_error_res();
}

std::string NotFoundService::get_url_prefix() {
	return this->url_prefix;
}