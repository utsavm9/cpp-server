#include "sessionSSL.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "config.h"
#include "handler.h"
#include "logger.h"

using boost::asio::ip::tcp;
namespace http = boost::beast::http;

sessionSSL::sessionSSL(ssl::context& ctx, NginxConfig* c, std::vector<std::pair<std::string, RequestHandler*>>& utoh, tcp::socket&& socket)
    : stream_(std::move(socket), ctx) {
	config = c;
	urlToHandler = utoh;
	name = "sessionSSL: ";
	TRACE << name << "constructed a new session";
	log_ip_address();
	begin = std::chrono::steady_clock::now();
}

sessionSSL::~sessionSSL() {
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::chrono::microseconds difference = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
	INFO << "metrics: " << name << " alive time (ms): " << difference.count();
	TRACE << name << "closed";
}

void sessionSSL::start() {
	TRACE << name << "starting a new session";
	// From Beast example code:
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session. Although not strictly necessary
	// for single-threaded contexts, this example code is written to be
	// thread-safe by default.

	// Executed is something related to a strand or io_context
	// Entire job of this middle-ware function is to inject this executor
	auto strand = stream_.get_executor();

	boost::asio::dispatch(strand,
	                      beast::bind_front_handler(&sessionSSL::do_handshake, shared_from_this()));

	TRACE << name << "finished dispatching the work to a strand";
}

void sessionSSL::do_handshake() {
	TRACE << name << "starting to do handshake";
	set_expiration(std::chrono::seconds(30));

	stream_.async_handshake(ssl::stream_base::server,
	                        beast::bind_front_handler(&sessionSSL::after_handshake, shared_from_this()));

	TRACE << name << "set the handler to run after handshake";
}

void sessionSSL::after_handshake(boost::beast::error_code err) {
	TRACE << name << "in after handshake handler";
	if (err) {
		TRACE << name << "error while doing handshake: " << err.message();
		return;
	}

	do_read();
}

void sessionSSL::async_read_stream() {
	// Asynchronously read out the request out of the stream with socket, use the buffer
	// for that and then place everything in the request.
	// Once that is done, call on_read which is wrapped inside stream_read_handler.
	http::async_read(stream_, buffer_, req_,
	                 beast::bind_front_handler(&sessionSSL::on_read, shared_from_this()));
}

void sessionSSL::async_write_stream(bool close) {
	// Asynchronously write the response back to the stream so that it it sent
	// and then call finished_write().
	http::async_write(stream_, res_,
	                  beast::bind_front_handler(&sessionSSL::finished_write, shared_from_this(), close));
}

void sessionSSL::log_ip_address() {
	try {
		std::string ip_addr = stream_.next_layer().socket().remote_endpoint().address().to_string();
		INFO << "metrics: request IP: " << ip_addr;
	} catch (std::exception& e) {
		TRACE << name << "exception occurred while getting the IP address of socket: " << e.what();
	}
}

void sessionSSL::set_expiration(std::chrono::seconds s) {
	stream_.next_layer().expires_after(std::chrono::seconds(s));
}

boost::beast::error_code sessionSSL::shutdown_stream() {
	boost::beast::error_code err;
	stream_.next_layer().socket().shutdown(tcp::socket::shutdown_send, err);
	return err;
}