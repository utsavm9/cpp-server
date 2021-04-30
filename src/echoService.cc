#include "echoService.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;

EchoService::EchoService(std::string prefix)
    : url_prefix(prefix) {
}

std::string EchoService::make_response(http::request<http::string_body> req) {
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::ok);
	res.set(http::field::content_type, "text/plain");
	res.set(http::field::server, "koko.cs130.org");
	res.body() = to_string(req);
	res.prepare_payload();
	return Service::to_string(res);
}

bool EchoService::can_handle(http::request<http::string_body> req) {
	std::string target(req.target());
	size_t prefix_len = url_prefix.size();

	if (prefix_len <= (target.size()) && target.substr(0, prefix_len) == url_prefix) {
		return true;
	} else {
		BOOST_LOG_SEV(slg::get(), info) << "denying to serve request";
		return false;
	}
}
