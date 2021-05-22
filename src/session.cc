#include "session.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "config.h"
#include "handler.h"
#include "logger.h"

using boost::asio::ip::tcp;
using error_code = boost::system::error_code;
namespace http = boost::beast::http;

session::session(NginxConfig* c, std::vector<std::pair<std::string, RequestHandler*>>& utoh, tcp::socket&& socket)
    : config(c),
      urlToHandler(utoh),
      stream_(std::move(socket)) {
	TRACE << "session: constructed a new session";

	try {
		std::string ip_addr = stream_.socket().remote_endpoint().address().to_string();
		INFO << "metrics: request IP: " << ip_addr;
	} catch (std::exception& e) {
		TRACE << "Exception occurred while getting the IP address of socket: " << e.what();
	}
}

session::~session() {
	TRACE << "session: closed";
}

void session::start() {
	TRACE << "session: starting a new session";
	// From Beast example code:
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session. Although not strictly necessary
	// for single-threaded contexts, this example code is written to be
	// thread-safe by default.

	// Executed is something related to a strand or io_context
	// Entire job of this middle-ware function is to inject this executor
	auto strand = stream_.get_executor();

	// this pointer of this pointer, wrapped in a shared_ptr
	std::shared_ptr<session> this_pointer = shared_from_this();

	// this->do_read() will be called once the strand is set-up.
	auto stand_start_handler = beast::bind_front_handler(&session::do_read, this_pointer);

	// This will submit handle_read for execution to the strand (thread+mutex object)
	// See https://stackoverflow.com/q/67542333/4726618 on why hanlder needs to be moved.
	boost::asio::dispatch(strand, std::move(stand_start_handler));

	TRACE << "session: finished dispatching the work to a strand";
}

void session::do_read() {
	TRACE << "session: starting work in a strand";

	// Make the request empty before reading,
	// otherwise the operation behavior is undefined.
	req_ = {};

	// Set the timeout.
	stream_.expires_after(std::chrono::seconds(30));

	// this pointer of this pointer, wrapped in a shared_ptr
	std::shared_ptr<session> this_pointer = shared_from_this();

	// Storing the intent to call this->on_read() or on_read(this) in a variable
	auto stream_read_handler = beast::bind_front_handler(&session::on_read, this_pointer);

	// Asynchronously read out the request out of the stream with socket, use the buffer
	// for that and then place everything in the request.
	// Once that is done, call on_read which is wrapped inside stream_read_handler.
	http::async_read(stream_, buffer_, req_, std::move(stream_read_handler));
}

void session::on_read(beast::error_code err, std::size_t bytes_transferred) {
	// async_read also passed us the number of bytes read.
	// It would have read all of the request and not some part of it.

	if (err == http::error::end_of_stream) {
		beast::error_code ec;
		stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
		TRACE << "session: stream ended, nothing read which needs processing";
		return;
	}

	INFO << "metrics: request path: " << req_.target();
	bool close = true;

	if (err) {
		close = true;
		res_ = RequestHandler::bad_request();
		ERROR << "session: error occurred while reading from the stream: " << err.message();
	} else {
		TRACE << "session: successfully read a request from the stream of size (bytes): " << bytes_transferred;

		// generate the response from the request stored as a data member in this session object
		// and store the response in another data member.
		construct_response(req_, res_);

		close = res_.need_eof();
	}

	INFO << "metrics: response code: " << res_.result_int();

	// this pointer of this object, wrapped in a shared_ptr
	std::shared_ptr<session> this_pointer = shared_from_this();

	// Wrap the intent to call this->finished_write() or finished_write(this)
	// Create a handler object whose job is to call the function we passed it.
	auto finished_write_handler = beast::bind_front_handler(&session::finished_write, this_pointer, close);

	// Asynchronously write the response back to the stream so that it it sent
	// and then call finished_write().
	http::async_write(stream_, res_, std::move(finished_write_handler));
}

void session::finished_write(bool close, beast::error_code err, std::size_t bytes_transferred) {
	if (err) {
		ERROR << "session: error occurred before finishing write: " << err.message();
		return;
	}

	if (close) {
		beast::error_code ec;
		stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
		TRACE << "session: stream ended";
		return;
	}

	TRACE << "session: finished writing a response, size (bytes): " << bytes_transferred;

	// Remove the response for the past request
	res_ = {};

	// we have finished a write, let us read another request from the same connection
	do_read();
}

void session::construct_response(http::request<http::string_body>& req, http::response<http::string_body>& res) {
	TRACE << "session: received " << req.method() << " request, user agent '" << req[http::field::user_agent] << "'";

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
		TRACE << "session: no request handler exists for " << req.method() << " request from user agent '" << req[http::field::user_agent] << "'";
		res = RequestHandler::not_found_error();
	} else {
		TRACE << "session: handler creating the response is mapped to: " << handler_url;
		INFO << "metrics: handler handling request: " << correct_handler->get_name();
		res = correct_handler->get_response(req);
	}
}