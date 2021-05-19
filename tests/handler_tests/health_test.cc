#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"
#include "server.h"
#include "healthHandler.h"

TEST(HealthHandlerTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("");
	p.Parse(&configStream, &config);
	HealthHandler test_handler("/", config);

	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);
	req.target("/health");
	std::string res = test_handler.to_string(test_handler.get_response(req));

	EXPECT_NE(res.find("200 OK"), std::string::npos);
}