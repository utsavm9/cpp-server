#include "handler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>

namespace http = boost::beast::http;
typedef boost::date_time::local_adjustor<boost::posix_time::ptime, -8, boost::posix_time::us_dst> us_pacific;

URLInfo::URLInfo(std::string u, int c, boost::posix_time::ptime t)
    : url(u),
      res_code(c),
      req_time(t) {
}

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

http::response<http::string_body> RequestHandler::get_response(const http::request<http::string_body>& request) {
	http::response<http::string_body> response = handle_request(request);

	//record url to response code pair
	boost::posix_time::ptime timeServerUTC = boost::posix_time::second_clock::universal_time();
	boost::posix_time::ptime timeCal = us_pacific::utc_to_local(timeServerUTC);
	RequestHandler::url_info.emplace_back(request.target().to_string(), response.result_int(), timeCal);

	return response;
}

std::vector<URLInfo> RequestHandler::url_info;

std::string RequestHandler::get_name() {
	return name;
}