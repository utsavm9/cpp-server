
#include "server.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <unordered_map>

#include "config.h"
#include "echoService.h"
#include "fileService.h"
#include "logger.h"
#include "service.h"
#include "session.h"

using boost::asio::ip::tcp;

server::server(boost::asio::io_context& io_context, NginxConfig c)
    : io_context_(io_context),
      config_(c),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), c.port)) {
	BOOST_LOG_SEV(slg::get(), info) << "server listening on port " << config_.port;

	// Setup service handlers
	for (auto p : config_.urlToServiceName) {
		std::string url_prefix = p.first;
		std::string service_name = p.second;

		// Echo service
		if (service_name == "echo") {
			service_handlers.push_back(new EchoService(url_prefix));
			BOOST_LOG_SEV(slg::get(), info) << "registered echo service for url prefix '" << url_prefix << "'";

			// Static service
		} else if (service_name == "static") {
			std::string linux_path = config_.urlToLinux[url_prefix];
			if (linux_path == "") {
				BOOST_LOG_SEV(slg::get(), info) << "url prefix '" << url_prefix << "' has no supporting linux path";
				continue;
			}
			service_handlers.push_back(new FileService(url_prefix, linux_path));
			BOOST_LOG_SEV(slg::get(), info) << "registered static service for url prefix '" << url_prefix << "'";

		} else {
			BOOST_LOG_SEV(slg::get(), error) << "unexpected service name '" << service_name << "' for url prefix '" << url_prefix << "' parsed from configs";
		}
	}
	BOOST_LOG_SEV(slg::get(), info) << "registered " << service_handlers.size() << " services";

	start_accept();
}

void server::start_accept() {
	session* new_session = new session(io_context_, &config_, service_handlers, 1024);
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