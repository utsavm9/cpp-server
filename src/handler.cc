#include "handler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

namespace http = boost::beast::http;

std::string RequestHandler::to_string(http::response<http::string_body> res) {
	std::stringstream res_str;
	res_str << res;
	return res_str.str();
}

std::string RequestHandler::to_string(http::request<http::string_body> req) {
	std::stringstream req_str;
	req_str << req;
	return req_str.str();
}

http::response<http::string_body> RequestHandler::bad_request() {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::bad_request);
	res.set(http::field::server, "koko.cs130.org");
	res.set(http::field::content_type, "text/plain");
	res.body() = "Request was malformed.";
	res.prepare_payload();
	return res;
}

http::response<http::string_body> RequestHandler::not_found_error() {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::not_found);
	res.set(http::field::server, "koko.cs130.org");
	res.set(http::field::content_type, "text/plain");
	res.body() = "The requested resource was not found.";
	res.prepare_payload();
	return res;
}

http::response<http::string_body> RequestHandler::internal_server_error() {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::internal_server_error);
	res.set(http::field::server, "koko.cs130.org");
	res.set(http::field::content_type, "text/plain");
	res.body() = "An internal error occurred on the server. Utsav will be fired promptly.";
	res.prepare_payload();
	return res;
}
