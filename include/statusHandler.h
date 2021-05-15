#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>
#include <vector>

#include "config.h"
#include "handler.h"

namespace http = boost::beast::http;

class StatusHandler : public RequestHandler {
   public:
	StatusHandler(const std::string& url_prefix, const NginxConfig& config);
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request);
    
};