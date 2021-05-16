#pragma once

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <vector>

#include "config.h"
#include "handler.h"

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

// Adapted from the session class from
// the official Boost Beast library examples
// https://www.boost.org/doc/libs/develop/libs/beast/example/http/server/async/http_server_async.cpp

class session : public std::enable_shared_from_this<session> {
   public:
	// The constructor taken in ownership of the stream by making
	// this object take ownership of the socket.
	// && means that socket was passed from a std::move, which means while no copies
	// were created, the socket is no longer accessible from the server.
	session(NginxConfig *c, std::vector<std::pair<std::string, RequestHandler *>> &utoh, tcp::socket &&socket);

	// To log when sessions are being destroyed
	~session();

	// Assigns a strand to take care of this object's execution
	// Is called once by the server and then control loops between
	// do_read → on_read → finished_write and back to do_read again.
	void start();

	// Given the request object, find the right handler and fills in the response
	void construct_response(http::request<http::string_body> &req, http::response<http::string_body> &res);

   private:
	// Assigns that a asynchronously read should happen when we receive a request
	// and that on_read should be called once that happens
	void do_read();

	// Constructs the response from the request, and then assigns a
	// asynchronous write that should happen to send the response back
	// Also assigns that finished_write should be called after we
	// write/send the response.
	void on_read(beast::error_code err, std::size_t bytes_transferred);

	// Clears the response and starts another read by calling do_read
	void finished_write(bool close, beast::error_code err, std::size_t bytes_transferred);

	NginxConfig *config;
	std::vector<std::pair<std::string, RequestHandler *>> urlToHandler;

	beast::tcp_stream stream_;
	http::request<http::string_body> req_;
	http::response<http::string_body> res_;
	beast::flat_buffer buffer_;
};
