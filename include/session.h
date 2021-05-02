#pragma once

#include <boost/asio.hpp>
#include <vector>

#include "config.h"
#include "service.h"

using boost::asio::ip::tcp;

class session {
   public:
	session(boost::asio::io_context &io_context, NginxConfig *c, std::vector<Service *> &s, int max_len);
	~session();

	tcp::socket &socket();
	void start();
	int get_max_length();

	std::string construct_response(size_t bytes_transferred);
	void change_data(std::string new_data);

   private:
	void handle_read(const boost::system::error_code &, size_t);
	void handle_write(const boost::system::error_code &);

	tcp::socket socket_;

	NginxConfig *config;
	std::vector<Service *> &service_handlers;

	int max_length;
	char *data_;
};
