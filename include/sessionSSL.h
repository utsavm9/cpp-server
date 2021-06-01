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
namespace ssl = boost::asio::ssl;

// Adapted from the session class from
// the official Boost Beast library examples
// https://www.boost.org/doc/libs/master/libs/beast/example/http/server/async-ssl/http_server_async_ssl.cpp

class sessionSSL : public session, public std::enable_shared_from_this<sessionSSL> {
   public:
	sessionSSL(ssl::context &ctx, NginxConfig *c, std::vector<std::pair<std::string, RequestHandler *>> &utoh, tcp::socket &&socket);

	~sessionSSL();

	// Assigns a strand to take care of this object's execution
	// Is called once by the server and then control loops between
	// do_handshake → after_handshake → do_read → on_read → finished_write and back to do_read again.
	virtual void start() override;

	// Runs before handshake is complete
	// and runs a async handshake at the end
	void do_handshake();

	// Runs after handshake has finished, logs errors
	// or moves on to read - write cycle
	void after_handshake(boost::beast::error_code err);

	// Provides async read and writes for SSL type streams
	virtual void async_read_stream() override;
	virtual void async_write_stream(bool close) override;

   protected:
	virtual void log_ip_address() override;
	virtual void set_expiration(std::chrono::seconds s) override;
	virtual boost::beast::error_code shutdown_stream() override;

	beast::ssl_stream<beast::tcp_stream> stream_;
};
