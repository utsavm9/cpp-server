#pragma once

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl.hpp>
#include <vector>

#include "config.h"
#include "handler.h"

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

class session {
   public:
	// Assigns a strand to take care of this object's execution
	// Should be called at thr start of the session for the session
	// to take ownership of itself
	virtual void start() = 0;

	// Given the request object, find the right handler and fills in the response
	void construct_response(http::request<http::string_body> &req, http::response<http::string_body> &res);

	// Runs first and before a read happens
	void do_read();

	// Subclasses override http async_read based on their type of stream
	virtual void async_read_stream() = 0;

	// Runs after the read has finished and before the write.
	// Constructs the response from the request.
	void on_read(beast::error_code err, std::size_t bytes_transferred);

	// Subclasses override http async_write based on their type of stream
	virtual void async_write_stream(bool close) = 0;

	// Runs after the write is finished
	// Clears the response and starts another read by calling do_read
	void finished_write(bool close, beast::error_code err, std::size_t bytes_transferred);

   protected:
	// Stream-specific utility functions that subclasses need to
	// override based on the type of their stream
	virtual void log_ip_address() = 0;
	virtual void set_expiration(std::chrono::seconds s) = 0;
	virtual boost::beast::error_code shutdown_stream() = 0;

	// Log metric name
	std::string name;

	// Contains the entire server config
	NginxConfig *config;

	// Maps URLs to handler pointers
	std::vector<std::pair<std::string, RequestHandler *>> urlToHandler;

	// Request and response associated to this session
	http::request<http::string_body> req_;
	http::response<http::string_body> res_;

	// Intermediate buffer used for async read and write into the
	// request and response objects
	beast::flat_buffer buffer_;
};
