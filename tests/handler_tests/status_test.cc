#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "handler.h"
#include "logger.h"
#include "parser.h"
#include "server.h"
#include "statusHandler.h"

TEST(StatusHandlerTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("");
	p.Parse(&configStream, &config);
	StatusHandler test_handler("/", config);

	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	StatusHandler::url_info.emplace_back("/foo", 200, now);
	StatusHandler::url_info.emplace_back("/brick", 404, now);
	StatusHandler::url_info.emplace_back("/file/trash.txt", 200, now);

	server::urlToHandlerName.emplace_back("/foo", "FooHandler");
	server::urlToHandlerName.emplace_back("/", "NotFoundHandler");
	server::urlToHandlerName.emplace_back("/file", "FileHandler");
	server::urlToHandlerName.emplace_back("/status", "StatusHandler");

	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);
	req.target("/status");
	std::string res = test_handler.to_string(test_handler.get_response(req));

	//test request to response code table is set correctly
	EXPECT_NE(res.find("Total Requests Received: 4"), std::string::npos);
	EXPECT_NE(res.find("/foo</td><td>200"), std::string::npos);
	EXPECT_NE(res.find("/brick</td><td>404"), std::string::npos);
	EXPECT_NE(res.find("/file/trash.txt</td><td>200"), std::string::npos);
	EXPECT_NE(res.find("/status</td><td>200"), std::string::npos);

	//test url prefix to handler table is set correctly
	EXPECT_NE(res.find("/foo</td><td>FooHandler"), std::string::npos);
	EXPECT_NE(res.find("/</td><td>NotFoundHandler"), std::string::npos);
	EXPECT_NE(res.find("/file</td><td>FileHandler"), std::string::npos);
	EXPECT_NE(res.find("/status</td><td>StatusHandler"), std::string::npos);

	URLInfo top = StatusHandler::url_info[StatusHandler::url_info.size() - 1];

	//check get_response adds url and response code pair
	ASSERT_TRUE(top.url == "/status");
	ASSERT_TRUE(top.res_code == 200);

	StatusHandler::url_info.clear();
	server::urlToHandlerName.clear();
}