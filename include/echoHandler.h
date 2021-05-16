#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "config.h"
#include "handler.h"

namespace http = boost::beast::http;

class EchoHandler : public RequestHandler {
   public:
	EchoHandler(const std::string& url_prefix, const NginxConfig& config);

	std::string get_url_prefix();

   protected:
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request) override;

   private:
	// Serve for these url suffixes. eg. "/" to serve all valid targets
	std::string url_prefix;
};