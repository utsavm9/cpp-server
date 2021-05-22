#include "sleepEchoHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;

SleepEchoHandler::SleepEchoHandler(const std::string& p, __attribute__((unused)) const NginxConfig& config)
    : EchoHandler(p, config) {
	name = "SleepEcho";
}

http::response<http::string_body> SleepEchoHandler::handle_request(const http::request<http::string_body>& request) {
	boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));

	http::response<http::string_body> res;
	res = EchoHandler::handle_request(request);
	return res;
}