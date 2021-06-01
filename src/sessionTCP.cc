#include "sessionTCP.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "config.h"
#include "handler.h"
#include "logger.h"

using boost::asio::ip::tcp;
namespace http = boost::beast::http;

sessionTCP::sessionTCP(NginxConfig* c, std::vector<std::pair<std::string, RequestHandler*>>& utoh, tcp::socket&& socket)
    : stream_(std::move(socket)) {
	config = c;
	urlToHandler = utoh;
	name = "sessionTCP: ";
	TRACE << name << "constructed a new session";
	log_ip_address();
}

sessionTCP::~sessionTCP() {
	TRACE << name << "closed";
}

void sessionTCP::start() {
	TRACE << name << "starting a new session";
	// From Beast example code:
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session. Although not strictly necessary
	// for single-threaded contexts, this example code is written to be
	// thread-safe by default.

	// Executed is something related to a strand or io_context
	// Entire job of this middle-ware function is to inject this executor
	auto strand = stream_.get_executor();

	// this pointer of this pointer, wrapped in a shared_ptr
	std::shared_ptr<session> this_pointer = shared_from_this();

	// this->do_read() will be called once the strand is set-up.
	auto stand_start_handler = beast::bind_front_handler(&session::do_read, this_pointer);

	// This will submit handle_read for execution to the strand (thread+mutex object)
	// See https://stackoverflow.com/q/67542333/4726618 on why hanlder needs to be moved.
	boost::asio::dispatch(strand, std::move(stand_start_handler));

	TRACE << name << "finished dispatching the work to a strand";
}

void sessionTCP::async_read_stream() {
	// Asynchronously read out the request out of the stream with socket, use the buffer
	// for that and then place everything in the request.
	// Once that is done, call on_read which is wrapped inside stream_read_handler.
	http::async_read(stream_, buffer_, req_,
	                 beast::bind_front_handler(&sessionTCP::on_read, shared_from_this()));
}

void sessionTCP::async_write_stream(bool close) {
	// Asynchronously write the response back to the stream so that it it sent
	// and then call finished_write().
	http::async_write(stream_, res_,
	                  beast::bind_front_handler(&sessionTCP::finished_write, shared_from_this(), close));
}

void sessionTCP::log_ip_address() {
	try {
		std::string ip_addr = stream_.socket().remote_endpoint().address().to_string();
		INFO << name << "request IP: " << ip_addr;
	} catch (std::exception& e) {
		TRACE << name << "exception occurred while getting the IP address of socket: " << e.what();
	}
}

void sessionTCP::set_expiration(std::chrono::seconds s) {
	stream_.expires_after(std::chrono::seconds(s));
}

boost::beast::error_code sessionTCP::shutdown_stream() {
	boost::beast::error_code err;
	stream_.socket().shutdown(tcp::socket::shutdown_send, err);
	return err;
}