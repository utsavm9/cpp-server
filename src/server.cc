
#include "server.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/bind.hpp>
#include <cstddef>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "compressedFileHandler.h"
#include "config.h"
#include "echoHandler.h"
#include "fileHandler.h"
#include "handler.h"
#include "healthHandler.h"
#include "logger.h"
#include "notFoundHandler.h"
#include "proxyRequestHandler.h"
#include "sessionSSL.h"
#include "sessionTCP.h"
#include "sleepEchoHandler.h"
#include "statusHandler.h"

using boost::asio::ip::tcp;
using boost::beast::error_code;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace fs = boost::filesystem;

std::vector<std::pair<std::string, std::string>> server::urlToHandlerName{};

// Retuns true if function is able to load the SSL context with the certificate and
// private key from the paths in the config
inline bool load_server_certificate(boost::asio::ssl::context& ctx, NginxConfig& config) {
	// Get paths
	std::string cert_path = config.get_str("certificate");
	std::string key_path = config.get_str("privateKey");
	if (cert_path == "") {
		ERROR << "server: could not get certificate path from config";
		return false;
	}
	if (key_path == "") {
		ERROR << "server: could not get private key path from config";
		return false;
	}

	// Check paths
	fs::path cert(cert_path);
	fs::path key(key_path);
	if (!(fs::exists(cert_path) && fs::is_regular_file(cert_path))) {
		ERROR << "server: certificate file does not exist, path: " << cert;
		return false;
	}
	if (!(fs::exists(key_path) && fs::is_regular_file(key_path))) {
		ERROR << "server: private key file does not exist, path: " << key;
		return false;
	}

	// Get contents of file
	std::ifstream cert_file(cert_path);
	std::ifstream key_file(key_path);
	std::string cert_content;
	cert_content.assign((std::istreambuf_iterator<char>(cert_file)), std::istreambuf_iterator<char>());
	std::string key_content;
	key_content.assign((std::istreambuf_iterator<char>(key_file)), std::istreambuf_iterator<char>());

	TRACE << "server: using certificate: " << cert_path;
	TRACE << "server: using private key: " << key_path;

	// Use file contents to setup SSL context
	try {
		ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2);
		ctx.use_certificate_chain(boost::asio::buffer(cert_content.data(), cert_content.size()));
		ctx.use_private_key(
		    boost::asio::buffer(key_content.data(), key_content.size()),
		    ssl::context::file_format::pem);
	} catch (std::exception& e) {
		ERROR << "server: could not make SSL context from given certificate and private key, exception: " << e.what();
		return false;
	}

	return true;
}


// Opens and binds an acceptor to the protocol and port specified in the endpoint
// Returns false on any failure and returns true otherwise
bool bind_acceptor(tcp::acceptor& acceptor, tcp::endpoint endpoint) {
	error_code err;
	acceptor.open(endpoint.protocol(), err);
	if (err) {
		ERROR << "server: could not open port " << endpoint.port() << ": " << err.message();
		return false;
	}

	// Allow address reuse
	acceptor.set_option(net::socket_base::reuse_address(true), err);
	if (err) {
		ERROR << "server: could not reuse address: " << err.message();
		return false;
	}

	// Bind to the server address
	acceptor.bind(endpoint, err);
	if (err) {
		ERROR << "server: could not bind to port " << endpoint.port() << ": " << err.message();
		return false;
	}

	// Start listening for connections
	acceptor.listen(net::socket_base::max_listen_connections, err);
	if (err) {
		ERROR << "server: could not listen on port " << endpoint.port() << ": " << err.message();
		return false;
	}

	TRACE << "server: successfully binded the acceptor to port: " << endpoint.port();
	return true;
}

server::server(boost::asio::io_context& io_context, ssl::context& ctx, NginxConfig c)
    : io_context_(io_context),
      ctx_(ctx),
      config_(c),
      port_(config_.get_num("port")),
      https_port_(config_.get_num("httpsPort")),
      acceptor_(io_context_),
      https_acceptor_(io_context_) {
	TRACE << "server: constructed";
	serving_ = false;
	serving_https_ = false;

	TRACE << "server: attempting to bind to HTTP port: " << port_;
	if (bind_acceptor(acceptor_, tcp::endpoint{tcp::v4(), port_})) {
		serving_ = true;
	}

	TRACE << "server: attempting to bind to HTTPS port: " << https_port_;
	if (bind_acceptor(https_acceptor_, tcp::endpoint{tcp::v4(), https_port_})) {
		serving_https_ = true;
	}

	urlToHandler_ = create_all_handlers(config_);
}

RequestHandler* server::create_handler(std::string url_prefix, std::string handler_name, NginxConfig subconfig) {
	if (handler_name == "EchoHandler") {
		TRACE << "server: registering echo handler for url prefix: " << url_prefix;
		return new EchoHandler(url_prefix, subconfig);
	}

	else if (handler_name == "StaticHandler") {
		TRACE << "server: registering static handler for url prefix: " << url_prefix;
		return new FileHandler(url_prefix, subconfig);
	}

	else if (handler_name == "NotFoundHandler") {
		TRACE << "server: registering not found handler for url prefix: " << url_prefix;
		return new NotFoundHandler(url_prefix, subconfig);
	}

	else if (handler_name == "ProxyRequestHandler") {
		TRACE << "server: registering proxy request handler for url prefix: " << url_prefix;
		return new ProxyRequestHandler(url_prefix, subconfig);
	}

	else if (handler_name == "StatusHandler") {
		TRACE << "server: registering status handler for url prefix: " << url_prefix;
		return new StatusHandler(url_prefix, subconfig);
	}

	else if (handler_name == "SleepEchoHandler") {
		TRACE << "server: registering status handler for url prefix: " << url_prefix;
		return new SleepEchoHandler(url_prefix, subconfig);
	}

	else if (handler_name == "HealthHandler") {
		TRACE << "server: registering health handler for url prefix: " << url_prefix;
		return new HealthHandler(url_prefix, subconfig);
	}

	else if (handler_name == "CompressedFileHandler") {
		TRACE << "server: registering compressed file handler for url prefix: " << url_prefix;
		return new CompressedFileHandler(url_prefix, subconfig);
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

			// Remove trailing slash from URL
			if (url_prefix[url_prefix.size() - 1] == '/' && url_prefix != "/") {
				url_prefix = url_prefix.substr(0, url_prefix.size() - 1);
			}

			// Add starting slash in URL
			if (url_prefix.size() > 0 && url_prefix[0] != '/') {
				url_prefix.insert(0, "/");
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

void server::start_accepting() {
	if (!serving_) {
		TRACE << "server: server not serving HTTP connection on port: " << acceptor_.local_endpoint().port();
		return;
	}
	TRACE << "server: preparing to accept a connection on HTTP port";

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

void server::start_accepting_https() {
	if (!serving_https_) {
		TRACE << "server: server not serving HTTPS connection on port: " << https_acceptor_.local_endpoint().port();
		return;
	}
	TRACE << "server: preparing to accept a connection on HTTPS port";

	// Handle function to call once we establish a HTTPS connection
	auto strand = net::make_strand(io_context_);
	std::shared_ptr<server> this_pointer = shared_from_this();
	auto connection_handler = boost::beast::bind_front_handler(&server::handle_accept_https, this_pointer);

	// Start accepting a connection asynchrnously
	https_acceptor_.async_accept(strand, std::move(connection_handler));
}

void server::handle_accept(error_code err, tcp::socket socket) {
	// acceptor's async_accept passes the socket to this function when this
	// handler is called

	if (err) {
		ERROR << "server: error ocurred while accepting HTTP connection: " << err.message();
		return;
	}

	TRACE << "server: just accepted a HTTP connection, creating session for it";

	// Create a session as a shared pointer.
	// Cannot directly assign the object to a variable because then the session
	// would be deconstructed at the end of this functions scope, but session
	// needs to stay alive for longer.
	// When the session itself no longer needs its own this pointer
	// (which will happen after closing the session and not assigning any more future async calls)
	// the session will be automatically destroyed since it is inside a shared pointer.
	std::shared_ptr<sessionTCP> s = std::make_shared<sessionTCP>(&config_, urlToHandler_, std::move(socket));

	// Start the session, which will call do_read and read the data
	// out of the socket we passed. socket will be destroyed when this function
	// call ends, but session will take its time to make the response.
	// Therefore, data from socket must be read by the time
	// we exit this function call.
	s->start();

	// This thread should now accept new connections for another session again
	start_accepting();
}

void server::handle_accept_https(error_code err, tcp::socket socket) {
	if (err) {
		ERROR << "server: error ocurred while accepting HTTPS connection: " << err.message();
		return;
	}

	TRACE << "server: just accepted a HTTPS connection, creating session for it";

	std::shared_ptr<sessionSSL> s = std::make_shared<sessionSSL>(ctx_, &config_, urlToHandler_, std::move(socket));
	s->start();

	// Accept new HTTPS connections now
	start_accepting_https();
}

void server::register_server_sigint() {
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = server::server_sigint;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
}

void server::server_sigint(__attribute__((unused)) int s) {
	TRACE << "server: received SIGINT, ending execution";
	exit(130);
}

void server::serve_forever(boost::asio::io_context* io_context, NginxConfig& config) {
	TRACE << "server: setting up to serve forever";
	server::register_server_sigint();

	// The SSL context holds certificates
	ssl::context ssl_ctx{ssl::context::tlsv12};

	// Load the test/production certificates from the config
	bool loaded_certs = load_server_certificate(ssl_ctx, config);

	try {
		// We need to have at least one shared pointer to server always
		// otherwise enabled_shared_from_this will throw an exception.
		// So, instead of creating a object directly, we will create a shared_ptr for
		// the server.
		std::shared_ptr<server> s = std::make_shared<server>(*io_context, ssl_ctx, config);
		s->serving_https_ = loaded_certs;

		// Start server with port from config
		// This will schedule a function call in the event loop
		s->start_accepting();
		s->start_accepting_https();

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
