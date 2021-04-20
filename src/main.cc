// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "config_parser.h"
#include "server.h"
#include "session.h"

int main(int argc, const char* argv[]) {
	int port;
	parse_args(argc, argv, &port);

	// Handles exit error code
	boost::asio::io_context io_context;
	server::serve_forever(&io_context, port);
	return 0;
}
