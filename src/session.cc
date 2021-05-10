#include "session.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "config.h"
#include "logger.h"
#include "service.h"

using boost::asio::ip::tcp;
using error_code = boost::system::error_code;
namespace http = boost::beast::http;

session::session(boost::asio::io_context& io_context, NginxConfig* c, std::vector<std::pair<std::string, Service*>>& utoh, int max_len)
    : socket_(io_context), config(c), urlToServiceHandler(utoh), max_length(max_len) {
	INFO << "constructed a new session";
	data_ = new char[max_length];
}

session::~session() {
	delete[] data_;
	INFO << "session closed";
}

tcp::socket& session::socket() {
	return socket_;
}

void session::start() {
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
	                        boost::bind(&session::handle_read, this,
	                                    boost::asio::placeholders::error,
	                                    boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
	if (!error) {
		std::string http_response = construct_response(bytes_transferred);

		// write http response to client
		boost::asio::async_write(socket_,
		                         boost::asio::buffer(http_response, http_response.size()),
		                         boost::bind(&session::handle_write, this,
		                                     boost::asio::placeholders::error));
	} else {
		delete this;
	}
}

void session::handle_write(const boost::system::error_code& error) {
	if (!error) {
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
		                        boost::bind(&session::handle_read, this,
		                                    boost::asio::placeholders::error,
		                                    boost::asio::placeholders::bytes_transferred));
	} else {
		delete this;
	}
}

std::string session::construct_response(size_t bytes_transferred) {
	if ((int)bytes_transferred > max_length) {  //this method shouldn't be called with these parameters
		ERROR << "session received a request which was larger than the maximum it could handle, internal server error";
		return Service::internal_server_error();
	}

	std::string req_str = std::string(data_, data_ + bytes_transferred);
	http::request_parser<http::string_body> parser;
	http::response<http::string_body> res;
	error_code err;

	// Parse the request
	parser.put(boost::asio::buffer(req_str), err);

	// Malformed request
	if (err) {
		ERROR << "error while parsing request, " << err.category().name() << ": " << err.message() << ", echoing back request by default";
		return Service::bad_request();
	}

	http::request<http::string_body> req = parser.get();
	INFO << "received " << req.method() << " request, user agent '" << req[http::field::user_agent] << "'";

	std::string target(req.target());
	size_t longest_prefix_match = 0;
	std::string handler_url = "";
	Service* correct_handler = nullptr;

	//find correct handler (longest matching prefix)
	for (std::pair<std::string, Service*> service_mapping : urlToServiceHandler) {
		std::string service_url_prefix = service_mapping.first;
		size_t prefix_len = service_url_prefix.size();

		if (target.substr(0, prefix_len) == service_url_prefix && prefix_len > longest_prefix_match) {
			longest_prefix_match = prefix_len;
			correct_handler = service_mapping.second;
			handler_url = service_url_prefix;
		}
	}

	if (correct_handler) {
		INFO << "Handler mapped to '" << handler_url << "' is being used to create a response";
		http::response<http::string_body> res = correct_handler->handle_request(req);
		return Service::to_string(res);
	}

	INFO << "no service handler exists for " << req.method() << " request from user agent '" << req[http::field::user_agent] << "'";

	return Service::bad_request();
}

void session::change_data(std::string new_data) {
	strncpy(data_, new_data.c_str(), max_length);
}