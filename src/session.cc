#include "session.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <utility>

#include "config.h"
#include "handler.h"
#include "logger.h"

using boost::asio::ip::tcp;
using error_code = boost::system::error_code;
namespace http = boost::beast::http;

void session::do_read() {
	TRACE << name << "starting work in a strand";

	// Make the request empty before reading,
	// otherwise the operation behavior is undefined.
	req_ = {};

	// Set the timeout.
	set_expiration(std::chrono::seconds(30));

	async_read_stream();
}

void session::on_read(beast::error_code err, std::size_t bytes_transferred) {
	// async_read also passed us the number of bytes read.
	// It would have read all of the request and not some part of it.
	if (err == beast::error::timeout) {
		TRACE << name << "stream ended, timeout";
		return;
	}

	if (err == http::error::end_of_stream) {
		beast::error_code ec = shutdown_stream();
		TRACE << name << "stream ended, nothing read which needs processing, err: " << ec.message();
		return;
	}

	INFO << "metrics: request path: " << req_.target();

	bool close = false;
	if (err) {
		close = true;
		res_ = RequestHandler::bad_request();
		ERROR << name << "error occurred while reading from the stream: " << err.message();
	} else {
		TRACE << name << "successfully read a request from the stream of size (bytes): " << bytes_transferred;

		// generate the response from the request stored as a data member in this session object
		// and store the response in another data member.
		construct_response(req_, res_);

		if(res_.find(http::field::connection) != res_.end() && res_.at(http::field::connection).to_string() == "close"){
			TRACE << name << "force close in handler";
			close = true;
		}
		else {close = res_.need_eof();}
	}

	INFO << "metrics: response code: " << res_.result_int();

	// Asynchronously write the response back to the stream so that it it sent
	// and then call finished_write().
	async_write_stream(close);
}

void session::finished_write(bool close, beast::error_code err, std::size_t bytes_transferred) {
	if (err) {
		ERROR << name << "error occurred before finishing write: " << err.message();
		return;
	}

	if (close) {
		beast::error_code ec = shutdown_stream();
		if(ec)
			TRACE << name << "stream ended, err: " << ec.message();
		return;
	}

	TRACE << name << "finished writing a response, size (bytes): " << bytes_transferred;

	// Remove the response for the past request
	res_ = {};

	// we have finished a write, let us read another request from the same connection
	do_read();
}

void session::construct_response(http::request<http::string_body>& req, http::response<http::string_body>& res) {
	TRACE << name <<  "received " << req.method() << " request, user agent '" << req[http::field::user_agent] << "'";

	std::string target = std::string(req.target());

	// ignore characters after the first "?" that is after the last "/" in URL
	size_t last_slash_index = target.rfind("/");
	if (last_slash_index != std::string::npos) {
		size_t first_qmark_index = target.substr(last_slash_index).find("?");
		if (first_qmark_index != std::string::npos) {
			target.erase(first_qmark_index);
		}
	}

	// add / to make sure the directory name is the same
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

	if (correct_handler == nullptr) {
		TRACE << name << "no request handler exists for " << req.method() << " request from user agent '" << req[http::field::user_agent] << "'";
		res = RequestHandler::not_found_error();
	} else {
		TRACE << name << "handler creating the response is mapped to: " << handler_url;
		INFO << "metrics: handler handling request: " << correct_handler->get_name();
		res = correct_handler->get_response(req);
		if(correct_handler->keep_alive){
			res.set(http::field::connection, "keep-alive");
		}
		else {
			res.set(http::field::connection, "close");
		}
	}
}