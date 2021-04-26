#pragma once

#include <boost/asio.hpp>

#include "config.h"
#include "response_generator.h"

using boost::asio::ip::tcp;

class session {
   public:
	session(boost::asio::io_context& io_context, NginxConfig* c);
	~session();

	tcp::socket& socket();
	void start();

   private:
	void handle_read(const boost::system::error_code&, size_t);
	void handle_write(const boost::system::error_code&);

	tcp::socket socket_;

	NginxConfig* config;
	Response_Generator response_generator_;

	enum { max_length = 1024 };
	char data_[max_length + 1];
};
