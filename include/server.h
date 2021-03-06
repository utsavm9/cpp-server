#pragma once

#include <boost/asio.hpp>
#include <vector>

#include "config.h"
#include "handler.h"
#include "session.h"

// Adapted from the listener class from
// the official Boost Beast library examples
// https://www.boost.org/doc/libs/develop/libs/beast/example/http/server/async/http_server_async.cpp

// Extending from enabled_shared_from_this allows us to
// have a function called shared_from_this available, which
// would return this pointer of an object wrapped in a shared_ptr<>.
class server : public std::enable_shared_from_this<server> {
   public:
	// Requires a io_context to run, and a NginxConfig config from which
	// port number and request handlers can be extracted.
	server(boost::asio::io_context& io_context, boost::asio::ssl::context& ctx, NginxConfig config);

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

	static std::vector<std::pair<std::string, std::string>> urlToHandlerName;

	// Indicates if HTTP and HTTPS acceptors can be used or not
	bool serving_;
	bool serving_https_;

   private:
	// Prepare to accept a connection
	void start_accepting();
	void start_accepting_https();

	// Create a session and start it, and then prepare to accept another connection
	void handle_accept(boost::beast::error_code error, tcp::socket socket);
	void handle_accept_https(boost::beast::error_code error, tcp::socket socket);

	// Manage the event loop
	boost::asio::io_context& io_context_;

	// Stores the SSL context, which contains the certificates
	boost::asio::ssl::context& ctx_;

	// The config with tokens parsed from the file passed to the program
	NginxConfig config_;

	// HTTP Port the server is listening on
	short unsigned port_;

	// HTTPS Port the server is listening on
	short unsigned https_port_;

	// Manages the listen-bind-connect and other networking-related
	// concepts about our socket on the HTTP Port and HTTPS port respectively
	tcp::acceptor acceptor_;
	tcp::acceptor https_acceptor_;

	// Maps url prefixes to handler pointers
	std::vector<std::pair<std::string, RequestHandler*>> urlToHandler_;
};

// Loads the certificates from the config into the SSL context
bool load_server_certificate(boost::asio::ssl::context& ctx, NginxConfig& config);