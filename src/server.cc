
#include "server.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <unordered_map>

#include "config.h"
#include "echoService.h"
#include "fileService.h"
#include "logger.h"
#include "notfoundService.h"
#include "service.h"
#include "session.h"

using boost::asio::ip::tcp;

server::server(boost::asio::io_context& io_context, NginxConfig c)
    : io_context_(io_context),
      config_(c),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), c.get_port())) {
	INFO << "server listening on port " << c.get_port();

	// Making handlers based on config
	for (const auto& statement : config_.statements_) {
		auto tokens = statement->tokens_;

		if (tokens.size() >= 3 && tokens[0] == "location") {
			std::string service_name = tokens[2];

			std::string url_prefix = tokens[1];
			Service* current_handler = nullptr;

			if (service_name == "EchoHandler") {
				current_handler = new EchoService(url_prefix, *(statement->child_block_));
				urlToServiceHandler.push_back(std::make_pair(url_prefix, current_handler));
				INFO << "registered echo service for url prefix '" << url_prefix << "'";
			} else if (service_name == "StaticHandler") {
				current_handler = new FileService(url_prefix, *(statement->child_block_));
				urlToServiceHandler.push_back(std::make_pair(url_prefix, current_handler));
				INFO << "registered static service for url prefix '" << url_prefix << "'";
			} else if (service_name == "404Handler") {
				current_handler = new NotFoundService(url_prefix, *(statement->child_block_));
				urlToServiceHandler.push_back(std::make_pair(url_prefix, current_handler));
				INFO << "registered notfound service for url prefix '" << url_prefix << "'";
			} else {
				ERROR << "unexpected service name '" << service_name << "' parsed from configs";
			}
		}
	}

	start_accept();
}

void server::start_accept() {
	session* new_session = new session(io_context_, &config_, urlToServiceHandler, 1024);
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