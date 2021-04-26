#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "service.h"

namespace http = boost::beast::http;

class EchoService : public Service {
   public:
	EchoService(std::string prefix);

	std::string make_response(http::request<http::string_body> req);
	bool can_handle(http::request<http::string_body> req);

   private:
	// Serve for these url suffixes. eg. "/" to serve all valid targets
	std::string url_prefix;
};