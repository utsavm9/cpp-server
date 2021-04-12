// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include "config_parser.h"
#include "session.h"
#include "server.h"

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
