#include "server.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "config.h"
#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

void server_runner(boost::asio::io_context* io_context, NginxConfig config, bool* done) {
	server::serve_forever(io_context, config);
	*done = true;
}

void response_read_worker(beast::tcp_stream* stream, http::response<http::string_body>* response, bool* done) {
	beast::flat_buffer buffer;
	http::response<http::string_body> res;
	beast::error_code ec;

	http::read(*stream, buffer, res, ec);

	stream->socket().shutdown(tcp::socket::shutdown_both, ec);
	if (ec && ec != beast::errc::not_connected) {
		INFO << "Failed to connect to host";
		throw beast::system_error{ec};
	}
	res.prepare_payload();

	*response = res;
	*done = true;
}

TEST(ServerTest, ServeForever) {
	// Spawn the server
	boost::asio::io_context io_context;
	bool done = false;
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str(
	    "port 8000; # The port my server listens on\n"
	    "location /echo EchoHandler {}\n"
	    "location /print EchoHandler {}\n"
	    "location /static StaticHandler {\n"
	    "root ../data/static_data;\n"
	    "}\n"
	    "location / NotFoundHandler{} \n"
	    "location /invalid ErrorButServerShouldntCrash{}\n");
	p.Parse(&configStream, &config);
	ASSERT_EQ(config.get_port(), 8000);
	std::thread server_thread(server_runner, &io_context, config, &done);

	// Wait for server to start-up
	std::chrono::seconds wait_time(1);
	std::this_thread::sleep_for(wait_time);

	// Check that the server has not died
	EXPECT_TRUE(server_thread.joinable());
	EXPECT_FALSE(done);

	// Shut down everything, and wait for the server to stop
	io_context.stop();
	std::this_thread::sleep_for(wait_time);

	// Check that the server is done
	EXPECT_TRUE(server_thread.joinable());
	EXPECT_TRUE(done);
	if (server_thread.joinable() && done) {
		server_thread.join();
	}
}

TEST(ServerTest, SignalHandling) {
	// Test that the signal handler exists with the right return code
	ASSERT_EXIT(
	    {
		    server::server_sigint(0);
		    exit(0);
	    },
	    ::testing::ExitedWithCode(130), "");
}

//handler request behavior is covered in Integration Tests, these are for Handler Creation
TEST(ServerTest, HandlerCreation) {
	{
		RequestHandler* handler = nullptr;

		NginxConfigParser p;

		http::request<http::string_body> stubReq;
		{
			NginxConfig sub_config;
			std::istringstream configStream;
			configStream.str("\n");
			p.Parse(&configStream, &sub_config);
			handler = server::create_handler("/echo", "EchoHandler", sub_config);
			ASSERT_NO_THROW(handler->get_response(stubReq));
		}
		{
			NginxConfig sub_config;
			std::istringstream configStream;
			stubReq.target("/static/test.html");
			configStream.str("root ../data\n");
			p.Parse(&configStream, &sub_config);
			handler = server::create_handler("/static", "StaticHandler", sub_config);
			ASSERT_NO_THROW(handler->get_response(stubReq));
		}
		{
			NginxConfig sub_config;
			std::istringstream configStream;
			configStream.str("");
			p.Parse(&configStream, &sub_config);
			handler = server::create_handler("/", "NotFoundHandler", sub_config);
			ASSERT_NO_THROW(handler->get_response(stubReq));
		}

		{
			NginxConfig sub_config;
			std::istringstream configStream;
			configStream.str("dest www.washington.edu; port 80;");
			p.Parse(&configStream, &sub_config);
			handler = server::create_handler("/", "ProxyRequestHandler", sub_config);
			ASSERT_NO_THROW(handler->get_response(stubReq));
		}

		{
			NginxConfig sub_config;
			std::istringstream configStream;
			configStream.str("");
			p.Parse(&configStream, &sub_config);
			handler = server::create_handler("/", "StatusHandler", sub_config);
			ASSERT_NO_THROW(handler->get_response(stubReq));
		}

		{
			NginxConfig sub_config;
			std::istringstream configStream;
			configStream.str("");
			p.Parse(&configStream, &sub_config);
			handler = server::create_handler("/", "NotARealHandler", sub_config);
			EXPECT_EQ(handler, nullptr);
		}
	}

	{
		// Check constructor
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str(
		    "port 8080; # The port my server listens on\n"
		    "location /echo EchoHandler {}\n"
		    "location /print EchoHandler {}\n"
		    "location /static/ StaticHandler {\n"
		    "root ../data/static_data;\n"
		    "}\n"
		    "location / NotFoundHandler {} \n"
		    "location /invalid ErrorButServerShouldntCrash {}\n"
		    "location /missingHandlerName {}\n");
		p.Parse(&configStream, &config);

		auto mapping = server::create_all_handlers(config);
		EXPECT_EQ(mapping.size(), 4);
	}
}

TEST(ServerTest, MultiThreadTest) {
	// Spawn the server
	boost::asio::io_context io_context;
	bool done = false;
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str(
	    "port 9999; # The port my server listens on\n"
	    "location /echo EchoHandler {}\n"
	    "location /sleep SleepEchoHandler {}\n");
	p.Parse(&configStream, &config);
	ASSERT_EQ(config.get_port(), 9999);
	std::thread server_thread(server_runner, &io_context, config, &done);

	// Wait for server to start-up
	std::chrono::seconds wait_time(1);
	std::this_thread::sleep_for(wait_time);

	// Check that the server has not died
	EXPECT_TRUE(server_thread.joinable());
	EXPECT_FALSE(done);

	// Create requests
	http::request<http::string_body> sleep_req;
	sleep_req.method(http::verb::get);
	sleep_req.target("/sleep");
	sleep_req.version(11);

	http::request<http::string_body> echo_req;
	echo_req.method(http::verb::get);
	echo_req.target("/echo");
	echo_req.version(11);

	net::io_context sleep_ioc;
	tcp::resolver sleep_resolver(sleep_ioc);
	beast::tcp_stream sleep_stream(sleep_ioc);

	tcp::resolver::query query("localhost", "9999");
	auto results = sleep_resolver.resolve(query);
	sleep_stream.connect(results);

	http::write(sleep_stream, sleep_req);

	net::io_context echo_ioc;
	tcp::resolver echo_resolver(echo_ioc);
	beast::tcp_stream echo_stream(echo_ioc);

	results = echo_resolver.resolve(query);
	echo_stream.connect(results);

	http::write(echo_stream, echo_req);

	// Store response and time taken
	bool done_t1;
	bool done_t2;
	http::response<http::string_body> sleep_res;
	http::response<http::string_body> echo_res;

	// Send sleep_req first, then echo_req
	std::thread t1(response_read_worker, &sleep_stream, &sleep_res, &done_t1);
	std::thread t2(response_read_worker, &echo_stream, &echo_res, &done_t2);

	// ensure echo is done before sleep_echo
	while (!(done_t1 || done_t2)) {
		continue;
	}

	EXPECT_TRUE(done_t2);
	EXPECT_FALSE(done_t1);

	// Join the threads
	t1.join();
	t2.join();

	// Check response is 200 OK
	std::string s;
	std::stringstream res_str;

	res_str << echo_res;
	s = res_str.str();
	EXPECT_NE(s.find("200 OK"), std::string::npos);
	EXPECT_NE(s.find("echo"), std::string::npos);

	res_str << sleep_res;
	s = res_str.str();
	EXPECT_NE(s.find("200 OK"), std::string::npos);
	EXPECT_NE(s.find("sleep"), std::string::npos);

	// Shut down everything, and wait for the server to stop
	io_context.stop();
	std::this_thread::sleep_for(wait_time);

	// Check that the server is done
	EXPECT_TRUE(server_thread.joinable());
	EXPECT_TRUE(done);
	if (server_thread.joinable() && done) {
		server_thread.join();
	}
}