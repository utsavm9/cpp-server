#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
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

	StatusHandler::url_to_res_code.push_back({"/foo", "200"});
	StatusHandler::url_to_res_code.push_back({"/brick", "404"});
	StatusHandler::url_to_res_code.push_back({"/file/trash.txt", "200"});

	server::urlToHandlerName.push_back({"/foo", "FooHandler"});
	server::urlToHandlerName.push_back({"/", "NotFoundHandler"});
	server::urlToHandlerName.push_back({"/file", "FileHandler"});
	server::urlToHandlerName.push_back({"/status", "StatusHandler"});

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

	std::pair<std::string, std::string> top = StatusHandler::url_to_res_code[StatusHandler::url_to_res_code.size() - 1];

	//check get_response adds url and response code pair
	ASSERT_TRUE(top.first == "/status");
	ASSERT_TRUE(top.second == "200");

	StatusHandler::url_to_res_code.clear();
	server::urlToHandlerName.clear();
}