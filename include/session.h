#pragma once 

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class session {
   public:
	session(boost::asio::io_service& io_service)
	    : socket_(io_service) {
	}

	tcp::socket& socket();
	void start();

   private:
	void handle_read(const boost::system::error_code&, size_t);
	void handle_write(const boost::system::error_code&);

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

	// strings used to create http response
	const char* crlf = "\r\n";
	const char* http_ok = "HTTP/1.1 200 OK";
	const char* content_type = "Content-Type: text/plain";
	const char* content_length = "Content-Length: ";
};
