
#include "server.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <unordered_map>

#include "config.h"
#include "logger.h"
#include "session.h"

using boost::asio::ip::tcp;

server::server(boost::asio::io_context& io_context, NginxConfig c)
    : io_context_(io_context),
      config_(c),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), c.port)) {
	BOOST_LOG_SEV(slg::get(), info) << "server listening on port " << config_.port;
	start_accept();
}

void server::start_accept() {
	session* new_session = new session(io_context_, &config_);
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

void server::server_sigint(int s) {
	BOOST_LOG_SEV(slg::get(), warning) << "received SIGINT, ending execution";
	exit(130);
}

void server::serve_forever(boost::asio::io_context* io_context, NginxConfig& config) {
	BOOST_LOG_SEV(slg::get(), info) << "setting up server to serve forever";
	server::register_server_sigint();

	try {
		// Start server with port from config
		server s(*io_context, config);
		io_context->run();
	} catch (std::exception& e) {
		BOOST_LOG_SEV(slg::get(), fatal) << "exception: " << e.what();
	}
}