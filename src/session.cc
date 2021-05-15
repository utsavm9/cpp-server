#include "session.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "config.h"
#include "handler.h"
#include "logger.h"

using boost::asio::ip::tcp;
using error_code = boost::system::error_code;
namespace http = boost::beast::http;

session::session(boost::asio::io_context& io_context, NginxConfig* c, std::vector<std::pair<std::string, RequestHandler*>>& utoh, int max_len)
    : socket_(io_context), config(c), urlToHandler(utoh), max_length(max_len) {
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
		return RequestHandler::to_string(RequestHandler::internal_server_error());
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
		return RequestHandler::to_string(RequestHandler::bad_request());
	}

	http::request<http::string_body> req = parser.get();
	INFO << "received " << req.method() << " request, user agent '" << req[http::field::user_agent] << "'";

	// add / to make sure the directory name is the same
	std::string target = std::string(req.target());
	target = target + "/";

	size_t longest_prefix_match = 0;
	std::string handler_url = "";
	RequestHandler* correct_handler = nullptr;

	//find correct handler (longest matching prefix)
	for (std::pair<std::string, RequestHandler*> handler_mapping : urlToHandler) {
		std::string handler_url_prefix = handler_mapping.first;
		size_t prefix_len = handler_url_prefix.size();

		bool longest_prefix_string_match = target.substr(0, prefix_len) == handler_url_prefix && prefix_len > longest_prefix_match;
		bool last_dirname_not_a_substr = (target[prefix_len] == '/') || handler_url_prefix == "/";

		if (longest_prefix_string_match && last_dirname_not_a_substr) {
			longest_prefix_match = prefix_len;
			correct_handler = handler_mapping.second;
			handler_url = handler_url_prefix;
		}
	}

	if (correct_handler) {
		INFO << "Handler mapped to '" << handler_url << "' is being used to create a response";
		http::response<http::string_body> res = correct_handler->get_response(req);
		return RequestHandler::to_string(res);
	}

	INFO << "no request handler exists for " << req.method() << " request from user agent '" << req[http::field::user_agent] << "'";

	return RequestHandler::to_string(RequestHandler::bad_request());
}

void session::change_data(std::string new_data) {
	strncpy(data_, new_data.c_str(), max_length);
}