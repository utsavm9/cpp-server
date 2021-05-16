#include "config.h"
#include "logger.h"
#include "parser.h"
#include "server.h"

int main(int argc, const char* argv[]) {
	init_logger();

	NginxConfig config;
	NginxConfigParser::parse_args(argc, argv, &config);

	// The event loop manager
	boost::asio::io_context io_context;
	server::serve_forever(&io_context, config);
	return 0;
}
