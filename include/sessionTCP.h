#pragma once

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <vector>

#include "config.h"
#include "handler.h"
#include "session.h"

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

// Adapted from the session class from
// the official Boost Beast library examples
// https://www.boost.org/doc/libs/develop/libs/beast/example/http/server/async/http_server_async.cpp

class sessionTCP : public session, public std::enable_shared_from_this<sessionTCP> {
   public:
	// && means that socket was passed from a std::move, which means while no copies
	// were created, the socket is no longer accessible from the server.
	sessionTCP(NginxConfig *c, std::vector<std::pair<std::string, RequestHandler *>> &utoh, tcp::socket &&socket);

	// To log when sessions are being destroyed
	~sessionTCP();

	// Assigns a strand to take care of this object's execution
	// Is called once by the server and then control loops between
	// do_read → on_read → finished_write and back to do_read again.
	virtual void start() override;

	virtual void async_read_stream() override;
	virtual void async_write_stream(bool close) override;

   protected:
	virtual void log_ip_address() override;
	virtual void set_expiration(std::chrono::seconds s) override;
	virtual boost::beast::error_code shutdown_stream() override;

	// Uses a simple TCP stream
	beast::tcp_stream stream_;
};
