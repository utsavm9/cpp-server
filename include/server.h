#pragma once

#include <boost/asio.hpp>
#include <vector>

#include "config.h"
#include "handler.h"
#include "session.h"

class server {
   public:
	// Requires a io_context to run, and a NginxConfig config from which
	// port number and request handlers can be extracted.
	server(boost::asio::io_context& io_context, NginxConfig config);

	// Needs modification EVERYTIME a new handler is registered.
	// Retuns a pointer to a newly constructed handler from the url prefix, handler name
	// and the config sub-block attached to a location block.
	// Returns a nullptr if no request handler could be created from the passed parameters
	static RequestHandler* create_handler(std::string url_prefix, std::string handler_name, NginxConfig subconfig);

	// Creates all handlers from config, returns the mappings
	static std::vector<std::pair<std::string, RequestHandler*>> create_all_handlers(NginxConfig config);

	// starts a server and block until an exception occurs
	static void serve_forever(boost::asio::io_context* io_context, NginxConfig& config);

	// Registers the server closing function to be run as server received SIGINT to shutdown
	static void register_server_sigint();

	// Server SIGINT handler, logs program and exists without calling any destructors
	static void server_sigint(__attribute__((unused)) int s);

   private:
	std::vector<std::pair<std::string, RequestHandler*>> urlToHandler;

	void start_accept();

	void handle_accept(session* new_session, const boost::system::error_code& error);

	boost::asio::io_context& io_context_;
	NginxConfig config_;
	tcp::acceptor acceptor_;
};
