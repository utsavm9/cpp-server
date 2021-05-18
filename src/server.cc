
#include "server.h"

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "echoHandler.h"
#include "fileHandler.h"
#include "handler.h"
#include "healthHandler.h"
#include "logger.h"
#include "notFoundHandler.h"
#include "proxyRequestHandler.h"
#include "session.h"
#include "sleepEchoHandler.h"
#include "statusHandler.h"

using boost::asio::ip::tcp;
using boost::beast::error_code;
namespace net = boost::asio;

server::server(boost::asio::io_context& io_context, NginxConfig c)
    : io_context_(io_context),
      config_(c),
      port_(config_.get_port()),
      acceptor_(io_context_, tcp::endpoint(tcp::v4(), port_)),
      urlToHandler_(create_all_handlers(config_)) {
	INFO << "server: constructed and listening on port " << port_;
}

RequestHandler* server::create_handler(std::string url_prefix, std::string handler_name, NginxConfig subconfig) {
	if (handler_name == "EchoHandler") {
		INFO << "server: registering echo handler for url prefix: " << url_prefix;
		return new EchoHandler(url_prefix, subconfig);
	}

	else if (handler_name == "StaticHandler") {
		INFO << "server: registering static handler for url prefix: " << url_prefix;
		return new FileHandler(url_prefix, subconfig);
	}

	else if (handler_name == "NotFoundHandler") {
		INFO << "server: registering not found handler for url prefix: " << url_prefix;
		return new NotFoundHandler(url_prefix, subconfig);
	}

	else if (handler_name == "ProxyRequestHandler") {
		INFO << "server: registering proxy request handler for url prefix: " << url_prefix;
		return new ProxyRequestHandler(url_prefix, subconfig);
	}

	else if (handler_name == "StatusHandler") {
		INFO << "server: registering status handler for url prefix: " << url_prefix;
		return new StatusHandler(url_prefix, subconfig);
	}

	else if (handler_name == "SleepEchoHandler") {
		INFO << "server: registering status handler for url prefix: " << url_prefix;
		return new SleepEchoHandler(url_prefix, subconfig);
	}

	else if (handler_name == "HealthHandler") {
		INFO << "server: registering health handler for url prefix: " << url_prefix;
		return new HealthHandler(url_prefix, subconfig);
	}
	ERROR << "server: unexpected handler name parsed from config: " << handler_name;
	return nullptr;
}

std::vector<std::pair<std::string, RequestHandler*>> server::create_all_handlers(NginxConfig config) {
	std::vector<std::pair<std::string, RequestHandler*>> temp_urlToHandler;

	// make handlers based on config
	for (const auto& statement : config.statements_) {
		auto tokens = statement->tokens_;

		if (tokens.size() > 0 && tokens[0] == "location") {
			if (tokens.size() != 3) {
				ERROR << "server: found a location block in config with incorrect syntax";
				continue;
			}

			std::string url_prefix = tokens[1];
			std::string handler_name = tokens[2];
			if (url_prefix[url_prefix.size() - 1] == '/' && url_prefix != "/") {
				url_prefix = url_prefix.substr(0, url_prefix.size() - 1);
			}

			NginxConfig* child_block = statement->child_block_.get();
			RequestHandler* s = server::create_handler(url_prefix, handler_name, *child_block);
			if (s != nullptr) {
				temp_urlToHandler.push_back({url_prefix, s});
				urlToHandlerName.push_back({url_prefix, handler_name});
			}
		}
	}
	return temp_urlToHandler;
}

void server::start_accept() {
	INFO << "server: preparing to accept a connection";

	// A strand is like a thread+mutex combination
	// This makes sure that while multiple write operation on DIFFERENT sockets (sessions?)
	// are happening in parallel, different write operations on the SAME socket (session?)
	// are happening in a serialized order.
	auto strand = net::make_strand(io_context_);

	// The this pointer of the current object
	std::shared_ptr<server> this_pointer = shared_from_this();

	// Storing the intent to call this->handle_accept() or handle_accept(this) in a variable
	// "bind" will wrap the this->handle_accept() call
	// "front_handler" allows handle_accpept to be called with variable number
	// of arguments which do not need to be known during compile time.
	auto connection_handler = boost::beast::bind_front_handler(&server::handle_accept, this_pointer);

	// Indicating that accpetor should call handle_accept when it accepts a connection.
	// See https://stackoverflow.com/q/67542333/4726618 for discussion on why connection_handler
	// uses std::move to be passed here.
	acceptor_.async_accept(strand, std::move(connection_handler));
}

void server::handle_accept(error_code err, tcp::socket socket) {
	// acceptor's async_accept passes the socket to this function when this
	// handler is called

	if (err) {
		ERROR << "server: error ocurred while accepting connection: " << err.message();
		return;
	}

	INFO << "server: just accepted a connection, creating session for it";

	// Create a session as a shared pointer.
	// Cannot directly assign the object to a variable because then the session
	// would be deconstructed at the end of this functions scope, but session
	// needs to stay alive for longer.
	// When the session itself no longer needs its own this pointer
	// (which will happen after closing the session and not assigning any more future async calls)
	// the session will be automatically destroyed since it is inside a shared pointer.
	std::shared_ptr<session> s = std::make_shared<session>(&config_, urlToHandler_, std::move(socket));

	// Start the session, which will call do_read and read the data
	// out of the socket we passed. socket will be destroyed when this function
	// call ends, but session will take its time to make the response.
	// Therefore, data from socket must be read by the time
	// we exit this function call.
	s->start();

	// This thread should now accept new connections for another session again
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
	INFO << "server: received SIGINT, ending execution";
	exit(130);
}

void server::serve_forever(boost::asio::io_context* io_context, NginxConfig& config) {
	INFO << "server: setting up to serve forever";
	server::register_server_sigint();

	try {
		// We need to have at least one shared pointer to server always
		// otherwise enabled_shared_from_this will throw an exception.
		// So, instead of creating a object directly, we will create a shared_ptr for
		// the server.
		std::shared_ptr<server> s = std::make_shared<server>(*io_context, config);

		// Start server with port from config
		// This will schedule a function call in the event loop
		s->start_accept();

		// Run server with 4 threads
		int threads = 4;
		std::vector<std::thread> threadpool;

		// We will also make the current parent thread do server work
		threadpool.reserve(threads - 1);

		auto thread_work = [io_context]() {
			// Makes the current thread a worker
			// for the event loop's async function calls.
			io_context->run();
		};

		for (int i = 0; i < threads - 1; ++i) {
			// Constructs threads with thread_work
			threadpool.emplace_back(thread_work);
		}

		// This thread will now forever keep finishing tasks in the event loop
		// All other functions in the server now have to registers handlers for
		// future events.
		io_context->run();

		// io_context was stopped, need to join all children
		for (auto& thread : threadpool) {
			if (thread.joinable()) {
				thread.join();
			}
		}
	} catch (std::exception& e) {
		FATAL << "server: exception occurred: " << e.what();
	}
}

std::vector<std::pair<std::string, std::string>> server::urlToHandlerName;