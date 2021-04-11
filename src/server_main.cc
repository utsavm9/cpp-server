//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <cstdlib>
#include <iostream>

#include "config_parser.h"

using boost::asio::ip::tcp;

class session {
   public:
	session(boost::asio::io_service& io_service)
	    : socket_(io_service) {
	}

	tcp::socket& socket() {
		return socket_;
	}

	void start() {
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
		                        boost::bind(&session::handle_read, this,
		                                    boost::asio::placeholders::error,
		                                    boost::asio::placeholders::bytes_transferred));
	}

   private:
	void handle_read(const boost::system::error_code& error,
	                 size_t bytes_transferred) {
		if (!error) {
			char http_response[max_length] = "\0"; // initialize http response

            // headers of http response
			strcat(http_response, http_ok);
			strcat(http_response, crlf);
			strcat(http_response, content_type);
			strcat(http_response, crlf);
			strcat(http_response, content_length);
			strcat(http_response, std::to_string(bytes_transferred).c_str()); // convert size_t to string and then to c string so that strcat works
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

	void handle_write(const boost::system::error_code& error) {
		if (!error) {
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
			                        boost::bind(&session::handle_read, this,
			                                    boost::asio::placeholders::error,
			                                    boost::asio::placeholders::bytes_transferred));
		} else {
			delete this;
		}
	}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

    // strings used to create http response
	const char* crlf = "\r\n";
	const char* http_ok = "HTTP/1.1 200 OK";
	const char* content_type = "Content-Type: text/plain";
	const char* content_length = "Content-Length: ";
};

class server {
   public:
	server(boost::asio::io_service& io_service, short port)
	    : io_service_(io_service),
	      acceptor_(io_service, tcp::endpoint(tcp::v4(), port)) {
		start_accept();
	}

   private:
	void start_accept() {
		session* new_session = new session(io_service_);
		acceptor_.async_accept(new_session->socket(),
		                       boost::bind(&server::handle_accept, this, new_session,
		                                   boost::asio::placeholders::error));
	}

	void handle_accept(session* new_session,
	                   const boost::system::error_code& error) {
		if (!error) {
			new_session->start();
		} else {
			delete new_session;
		}

		start_accept();
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
	try {
		if (argc != 2) {
			std::cerr << "Usage: webserver <config_file_path>\n";
			return 1;
		}

		// Parse the config
		NginxConfigParser config_parser;
		NginxConfig config;
		if (!config_parser.Parse(argv[1], &config)) {
			std::cerr << "could not parse the config" << std::endl;
		}

		// Extract the port number from config
		int port = config.find_port();
		std::cerr << "found port number " << port << std::endl;

		// Start server with port from config
		boost::asio::io_service io_service;
		server s(io_service, port);
		io_service.run();

	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
