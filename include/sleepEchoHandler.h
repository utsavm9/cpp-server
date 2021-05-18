#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <string>

#include "config.h"
#include "echoHandler.h"

namespace http = boost::beast::http;

class SleepEchoHandler : public EchoHandler {
   public:
	SleepEchoHandler(const std::string& url_prefix, const NginxConfig& config);

   protected:
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request) override;
};