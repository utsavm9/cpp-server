#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

namespace http = boost::beast::http;

class Service {
   public:
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request) = 0;
	// Returns a response as a string for the given request
	// Call can_handle first to ensure that this service wants to serve the request
	virtual std::string make_response(__attribute__((unused)) http::request<http::string_body> req);

	// Returns true if this service can make a 200 OK response for the given request
	virtual bool can_handle(__attribute__((unused)) http::request<http::string_body> req);

	// Returns a 400 bad request
	static std::string bad_request();

	// Returns a 404 not found response
	static std::string not_found_error();
	http::response<http::string_body> not_found_error_res();

	// Returns a 500 server error
	static std::string internal_server_error();

	// Converts the response into a string
	static std::string to_string(http::response<http::string_body> res);
	static std::string to_string(http::request<http::string_body> req);
};