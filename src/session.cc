#include "session.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "config.h"
#include "logger.h"

using boost::asio::ip::tcp;

session::session(boost::asio::io_context& io_context, NginxConfig* c)
    : socket_(io_context), config(c) {
	BOOST_LOG_SEV(slg::get(), info) << "constructed a new session";
}

session::~session() {
	BOOST_LOG_SEV(slg::get(), info) << "session closed";
}

tcp::socket& session::socket() {
	return socket_;
}

void session::start() {
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
	                        boost::bind(&session::handle_read, this,
	                                    boost::asio::placeholders::error,
	                                    boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
	if (!error) {
		std::string http_response = response_generator_.generate_response(data_, bytes_transferred, max_length + 1);  //last param length of buffer

		// write http response to client
		boost::asio::async_write(socket_,
		                         boost::asio::buffer(http_response, http_response.size()),
		                         boost::bind(&session::handle_write, this,
		                                     boost::asio::placeholders::error));
	} else {
		delete this;
	}
}

void session::handle_write(const boost::system::error_code& error) {
	if (!error) {
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
		                        boost::bind(&session::handle_read, this,
		                                    boost::asio::placeholders::error,
		                                    boost::asio::placeholders::bytes_transferred));
	} else {
		delete this;
	}
}
