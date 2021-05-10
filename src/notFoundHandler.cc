#include "notFoundHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;

NotFoundHandler::NotFoundHandler(const std::string& p, __attribute__((unused)) const NginxConfig& config)
    : url_prefix(p) {
}

http::response<http::string_body> NotFoundHandler::handle_request(__attribute__((unused)) const http::request<http::string_body>& request) {
	return not_found_error_res();
}