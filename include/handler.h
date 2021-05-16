#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>
#include <vector>

namespace http = boost::beast::http;

class RequestHandler {
   public:
	// Wraps handle_request and records url to response code pair
	http::response<http::string_body> get_response(const http::request<http::string_body>& request);

	// Returns a 400 bad request
	static http::response<http::string_body> bad_request();

	// Returns a 404 not found response
	static http::response<http::string_body> not_found_error();

	// Returns a 500 server error
	static http::response<http::string_body> internal_server_error();

	// Converts the response into a string
	static std::string to_string(http::response<http::string_body> res);
	static std::string to_string(http::request<http::string_body> req);


	static std::vector<std::pair<std::string, std::string>> url_to_res_code;

   protected:
	// Returns a response for the given request
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request) = 0;
};