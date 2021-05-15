#include "statusHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "handler.h"
#include "server.h"

namespace http = boost::beast::http;

StatusHandler::StatusHandler(__attribute__((unused)) const std::string& url_prefix, __attribute__((unused)) const NginxConfig& config) {
}

http::response<http::string_body> StatusHandler::handle_request(const http::request<http::string_body>& request) {
	auto url_to_res_code = RequestHandler::url_to_res_code;
	auto url_to_handler = server::urlToHandlerName;

	//setup html table content for url to response code
	std::string url_to_res_code_table = "<tr><td>" + std::string(request.target()) + "</td><td>200</td></tr>";

	for (int i = url_to_res_code.size() - 1; i >= 0; i--) {
		url_to_res_code_table +=
		    "<tr><td>" + url_to_res_code[i].first +
		    "</td><td>" + url_to_res_code[i].second + "</td></tr>";
	}

	std::string url_prefix_to_handler_table = "";

	for (size_t i = 0; i < url_to_handler.size(); i++) {
		url_prefix_to_handler_table +=
		    "<tr><td>" + url_to_handler[i].first +
		    "</td><td>" + url_to_handler[i].second + "</td></tr>";
	}

	std::string num_requests = std::to_string(RequestHandler::url_to_res_code.size() + 1);

	std::string html_template =
	    "<html>"
	    "<head>"
	    "<style>table{width = \"500\";}td{width = \"400\";}</style>"
	    "<title>Koko Status Report</title>"
	    "</head>"
	    "<body>"
	    "<h1>Status Report</h1>"
	    "<h3>Total Requests Received: " +
	    num_requests +
	    "</h3>"
        "</br>"
        "<h3>Received Requests</h3>"
	    "<table>"
	    "<tr><th>Location</th><th>Response Code</th></tr>" +
	    url_to_res_code_table +
	    "</table>"
        "</br>"
        "<h2>Registered Handlers</h2>"
        "<table>"
	    "<tr><th>URL Prefix</th><th>Handler Type</th></tr>" +
	    url_prefix_to_handler_table +
	    "</table>"
	    "</body>"
	    "</html>";

	//create response
	http::response<http::string_body> res;
	res.version(11);
	res.result(http::status::ok);
	res.set(http::field::content_type, "text/html");
	res.set(http::field::server, "koko.cs130.org");
	res.body() = html_template;
	res.prepare_payload();

	return res;
}