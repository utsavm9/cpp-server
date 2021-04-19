#pragma once

#include <boost/asio.hpp>

#include "response_generator.h"

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

	Response_Generator response_generator_;

	enum { max_length = 1024 };
	char data_[max_length + 1];
};
