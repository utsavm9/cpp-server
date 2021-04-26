#pragma once

#include <boost/asio.hpp>
#include <unordered_map>

#include "session.h"

class server {
   public:
	server(boost::asio::io_context& io_context, short port);

	// starts a server and block until an exception occurs
	static void serve_forever(boost::asio::io_context* io_context, int port, std::unordered_map<std::string, std::vector<std::string>>* path_map);

   private:
	void start_accept();

	void handle_accept(session* new_session, const boost::system::error_code& error);

	boost::asio::io_context& io_context_;
	tcp::acceptor acceptor_;
};
