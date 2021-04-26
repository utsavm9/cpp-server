#pragma once

#include <boost/asio.hpp>
#include <unordered_map>

#include "config.h"
#include "session.h"

class server {
   public:
	server(boost::asio::io_context& io_context, NginxConfig config);

	// starts a server and block until an exception occurs
	static void serve_forever(boost::asio::io_context* io_context, NginxConfig& config);

	// Registers the server closing function to be run as server received SIGINT to shutdown
	static void register_server_sigint();

	// Server SIGINT handler, logs program and exists without calling any destructors
	static void server_sigint(int s);

   private:
	void start_accept();

	void handle_accept(session* new_session, const boost::system::error_code& error);

	boost::asio::io_context& io_context_;
	tcp::acceptor acceptor_;
	NginxConfig config_;
};
