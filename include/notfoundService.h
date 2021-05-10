#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "config.h"
#include "service.h"

namespace http = boost::beast::http;

class NotFoundService : public Service {
   public:
	NotFoundService(const std::string& prefix);
	NotFoundService(const std::string& url_prefix, __attribute__((unused)) const NginxConfig& config);
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request);

	std::string get_url_prefix();

   private:
	// Serve for these url suffixes. eg. "/" to serve all valid targets
	std::string url_prefix;
};