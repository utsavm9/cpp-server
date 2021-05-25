#include "statusHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>

#include "handler.h"
#include "server.h"

namespace http = boost::beast::http;
typedef boost::date_time::local_adjustor<boost::posix_time::ptime, -8, boost::posix_time::us_dst> us_pacific;

StatusHandler::StatusHandler(__attribute__((unused)) const std::string& url_prefix, __attribute__((unused)) const NginxConfig& config) {
	name = "Status";
}

http::response<http::string_body> StatusHandler::handle_request(const http::request<http::string_body>& request) {
	auto url_info = RequestHandler::url_info;
	auto url_to_handler = server::urlToHandlerName;

	//setup html table content for url to response code
	std::stringstream url_info_table;
	boost::posix_time::ptime timeServerUTC = boost::posix_time::second_clock::universal_time();
	boost::posix_time::ptime timeLA = us_pacific::utc_to_local(timeServerUTC);
	url_info_table << "<tr><td>" << std::string(request.target()) << "</td><td>200</td>"
	               << "<td>" << timeLA << "</td>"
	               << "</tr>";

	for (int i = url_info.size() - 1; i >= 0; i--) {
		url_info_table << "<tr>"
		               << "<td>" << url_info[i].url << "</td>"
		               << "<td>" << url_info[i].res_code << "</td>"
		               << "<td>" << url_info[i].req_time << "</td>"
		               << "</tr>";
	}

	std::string url_prefix_to_handler_table = "";

	for (size_t i = 0; i < url_to_handler.size(); i++) {
		url_prefix_to_handler_table +=
		    "<tr><td>" + url_to_handler[i].first +
		    "</td><td>" + url_to_handler[i].second + "</td></tr>";
	}

	std::string num_requests = std::to_string(RequestHandler::url_info.size() + 1);

	std::string html_template =
	    "<html>"
	    "<head>"
	    "<style>table{width = \"500\";}td{width = \"400\";}</style>"
	    "<title>Koko Status Report</title>"
	    "</head>"
	    "<body>"
	    "<h1>Koko Server Status Report</h1>"
	    "Members: Bryan Nguyen, Hyounjun Chang, Tanmaya Hada, Utsav Munendra<br>"
	    "<h3>Total Requests Received: " +
	    num_requests +
	    "</h3>"
	    "</br>"
	    "<h3>Received Requests</h3>"
	    "<table>"
	    "<tr><th>Location</th><th>Response Code</th><th>Time (LA)</th></tr>" +
	    url_info_table.str() +
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