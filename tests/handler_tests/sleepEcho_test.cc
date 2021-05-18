#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "handler.h"
#include "logger.h"
#include "parser.h"
#include "server.h"
#include "sleepEchoHandler.h"

TEST(SleepEchoHandlerTest, UnitTests) {
	// Test Echo functionality

	{  // Check constructor
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		SleepEchoHandler esv("/sleep", config);
		EXPECT_EQ("/sleep", esv.get_url_prefix());
	}

	{
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("");
		p.Parse(&configStream, &config);
		SleepEchoHandler sesv("/sleep", config);

		std::string s;
		http::request<http::string_body> req;
		req.method(http::verb::get);
		req.target("/echo");
		req.version(11);

		// Check OK response
		s = sesv.to_string(sesv.get_response(req));
		EXPECT_NE(s.find("200 OK"), std::string::npos);
		EXPECT_NE(s.find("GET", 5), std::string::npos);
	}
}