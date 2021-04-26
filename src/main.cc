// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "config_parser.h"
#include "server.h"
#include "session.h"
#include "logger.h"

#include <csignal>
#include <vector>
#include <unordered_map>

void sigint_handler(int s){
	BOOST_LOG_SEV(slg::get(), warning) << "SIGINT, Closing Server....";
	exit(130);
}

int main(int argc, const char* argv[]) {

	struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sigint_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

	int port;
	std::unordered_map<std::string, std::vector<std::string>>* path_map;

	init_logger();
	parse_args(argc, argv, &port, path_map);
	
	// Handles exit error code
	boost::asio::io_context io_context;
	server::serve_forever(&io_context, port, path_map);
	return 0;
}
