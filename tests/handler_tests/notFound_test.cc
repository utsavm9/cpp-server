#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"
#include "server.h"
#include "notFoundHandler.h"

TEST(NotFoundHandlerTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("");
	p.Parse(&configStream, &config);
	NotFoundHandler esv("/", config);

	std::string s;
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.target("/");
	req.version(11);

	// Check OK response
	s = esv.to_string(esv.get_response(req));
	EXPECT_NE(s.find("404 Not Found"), std::string::npos);
}