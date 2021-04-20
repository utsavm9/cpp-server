
#include "server.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>

#include "session.h"

using boost::asio::ip::tcp;

server::server(boost::asio::io_context& io_context, short port)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
	start_accept();
}

void server::start_accept() {
	session* new_session = new session(io_context_);
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

void server::serve_forever(boost::asio::io_context* io_context, int port) {
	try {
		// Start server with port from config
		server s(*io_context, port);
		io_context->run();

	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}
}