#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"
#include "server.h"
#include "echoHandler.h"

TEST(EchoHandlerTest, UnitTests) {
	{  // Check constructor
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("root \"./files\";  # supports relative path");
		p.Parse(&configStream, &config);
		EchoHandler esv("/echo", config);
		EXPECT_EQ("/echo", esv.get_url_prefix());
	}

	{
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("");
		p.Parse(&configStream, &config);
		EchoHandler esv("/echo", config);

		std::string s;
		http::request<http::string_body> req;
		req.method(http::verb::get);
		req.target("/echo");
		req.version(11);

		// Check OK response
		s = esv.to_string(esv.get_response(req));
		EXPECT_NE(s.find("200 OK"), std::string::npos);
		EXPECT_NE(s.find("GET", 5), std::string::npos);
	}
}