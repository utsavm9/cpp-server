#include "session.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

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
		// Ensure that the data is null terminated
		if (bytes_transferred < 0) {
			data_[0] = '\0';
		} else if (bytes_transferred >= 1024) {
			data_[1024] = '\0';
		} else {
			data_[bytes_transferred] = '\0';
		}

		// Construct the response
		std::string http_response =
		    "HTTP/1.1 200 OK\r\n"
		    "Content-Type: text\\plain\r\n"
		    "Content-Length: " +
		    std::to_string(bytes_transferred) +
		    "\r\n"
		    "\r\n" +
		    std::string(data_);

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
