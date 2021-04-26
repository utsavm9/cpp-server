// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "config.h"
#include "logger.h"
#include "parser.h"
#include "server.h"

int main(int argc, const char* argv[]) {
	init_logger();

	NginxConfig config;
	NginxConfigParser::parse_args(argc, argv, &config);

	// Handles exit error code
	boost::asio::io_context io_context;
	server::serve_forever(&io_context, config);
	return 0;
}
