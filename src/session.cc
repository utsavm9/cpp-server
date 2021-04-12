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

void session::handle_read(const boost::system::error_code& error,
                          size_t bytes_transferred) {
	if (!error) {
		// initialize http response
		char http_response[max_length] = "\0";  

		// headers of http response
		strcat(http_response, http_ok);
		strcat(http_response, crlf);
		strcat(http_response, content_type);
		strcat(http_response, crlf);
		strcat(http_response, content_length);
		// convert size_t to string and then to c string so that strcat works
		strcat(http_response, std::to_string(bytes_transferred).c_str()); 
		strcat(http_response, crlf);

		// body of http response
		strcat(http_response, crlf);
		strcat(http_response, data_);

		// write http response to client
		boost::asio::async_write(socket_,
		                         boost::asio::buffer(http_response, strlen(http_response)),
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
