
#include "server.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <unordered_map>

#include "config.h"
#include "echoHandler.h"
#include "fileHandler.h"
#include "handler.h"
#include "logger.h"
#include "notFoundHandler.h"
#include "session.h"

using boost::asio::ip::tcp;

server::server(boost::asio::io_context& io_context, NginxConfig c)
    : io_context_(io_context),
      config_(c),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), c.get_port())) {
	INFO << "server listening on port " << c.get_port();

	// make handlers based on config
	for (const auto& statement : config_.statements_) {
		auto tokens = statement->tokens_;

		if (tokens.size() > 0 && tokens[0] == "location") {
			if (tokens.size() != 3) {
				ERROR << "found a location block in config with incorrect syntax";
			}

			std::string url_prefix = tokens[1];
			std::string handler_name = tokens[2];
			NginxConfig* child_block = statement->child_block_.get();
			RequestHandler* s = create_handler(url_prefix, handler_name, *child_block);
			if (s != nullptr) {
				urlToHandler.push_back({url_prefix, s});
			}
		}
	}

	start_accept();
}

RequestHandler* server::create_handler(std::string url_prefix, std::string handler_name, NginxConfig subconfig) {
	if (handler_name == "EchoHandler") {
		INFO << "registering echo handler for url prefix: " << url_prefix;
		return new EchoHandler(url_prefix, subconfig);
	}

	else if (handler_name == "StaticHandler") {
		INFO << "registering static handler for url prefix: " << url_prefix;
		return new FileHandler(url_prefix, subconfig);
	}

	else if (handler_name == "NotFoundHandler") {
		INFO << "registering not found handler for url prefix: " << url_prefix;
		return new NotFoundHandler(url_prefix, subconfig);
	}

	ERROR << "unexpected handler name parsed from config: " << handler_name;
	return nullptr;
}

void server::start_accept() {
	session* new_session = new session(io_context_, &config_, urlToHandler, 1024);
	acceptor_.async_accept(new_session->socket(),
	                       boost::bind(&server::handle_accept, this, new_session,
	                                   boost::asio::placeholders::error));
}

void server::handle_accept(session* new_session, const boost::system::error_code& error) {
	if (!error) {
		new_session->start();
	} else {
		delete new_session;
	}

	start_accept();
}

void server::register_server_sigint() {
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = server::server_sigint;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
}

void server::server_sigint(__attribute__((unused)) int s) {
	INFO << "received SIGINT, ending execution";
	exit(130);
}

void server::serve_forever(boost::asio::io_context* io_context, NginxConfig& config) {
	INFO << "setting up server to serve forever";
	server::register_server_sigint();

	try {
		// Start server with port from config
		server s(*io_context, config);
		io_context->run();
	} catch (std::exception& e) {
		FATAL << "exception: " << e.what();
	}
}