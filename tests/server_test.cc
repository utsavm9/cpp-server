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

void send_req_localhost(http::request<http::string_body>* request, http::response<http::string_body>* response, unsigned int* time_taken_in_ms, std::string port) {
	net::io_context ioc;
	tcp::resolver resolver(ioc);
	beast::tcp_stream stream(ioc);
	http::request<http::string_body> req;
	http::response<http::string_body> res;

	req = *request;

	tcp::resolver::query query("localhost", port);
	auto const results = resolver.resolve(query);
	stream.connect(results);

	// time the response time
	auto t_start = std::chrono::high_resolution_clock::now();
	http::write(stream, req);
	beast::flat_buffer buffer;

	beast::error_code ec;
	http::read(stream, buffer, res, ec);
	stream.socket().shutdown(tcp::socket::shutdown_both, ec);
	if (ec && ec != beast::errc::not_connected) {
		INFO << "Failed to connect to host";
		throw beast::system_error{ec};
	}
	res.prepare_payload();

	auto t_end = std::chrono::high_resolution_clock::now();
	*time_taken_in_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
	INFO << "Time taken for this request: " << *time_taken_in_ms << " ms\n";

	*response = res;
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
		NginxConfig sub_config;
		NginxConfigParser p;
		std::istringstream configStream;
		http::request<http::string_body> stubReq;

		configStream.str("\n");
		p.Parse(&configStream, &sub_config);
		handler = server::create_handler("/echo", "EchoHandler", sub_config);
		ASSERT_NO_THROW(handler->get_response(stubReq));

		configStream.str("root ../data\n");
		p.Parse(&configStream, &sub_config);
		handler = server::create_handler("/static", "StaticHandler", sub_config);
		ASSERT_NO_THROW(handler->get_response(stubReq));

		configStream.str("");
		p.Parse(&configStream, &sub_config);
		handler = server::create_handler("/", "NotFoundHandler", sub_config);
		ASSERT_NO_THROW(handler->get_response(stubReq));

		configStream.str("dest www.washington.edu; port 80;");
		p.Parse(&configStream, &sub_config);
		handler = server::create_handler("/", "ProxyRequestHandler", sub_config);
		ASSERT_NO_THROW(handler->get_response(stubReq));

		configStream.str("");
		p.Parse(&configStream, &sub_config);
		handler = server::create_handler("/", "StatusHandler", sub_config);
		ASSERT_NO_THROW(handler->get_response(stubReq));

		configStream.str("");
		p.Parse(&configStream, &sub_config);
		handler = server::create_handler("/", "NotARealHandler", sub_config);
		EXPECT_EQ(handler, nullptr);
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

	// Store response and time taken
	http::response<http::string_body> sleep_res;
	unsigned int sleep_res_time_in_ms;
	http::response<http::string_body> echo_res;
	unsigned int echo_res_time_in_ms;

	// Send sleep_req first, then echo_req
	std::thread t1(send_req_localhost, &sleep_req, &sleep_res, &sleep_res_time_in_ms, "9999");
	wait_time = std::chrono::seconds(1);
	std::this_thread::sleep_for(wait_time);
	std::thread t2(send_req_localhost, &echo_req, &echo_res, &echo_res_time_in_ms, "9999");

	// Join the threads
	t1.join();
	t2.join();

	// check sleep response time > sleep time
	EXPECT_TRUE(sleep_res_time_in_ms > 3000);
	// If server is not multi-threaded, echo_res is blocked for ~2000ms, failing this
	EXPECT_TRUE(echo_res_time_in_ms < 500);

	// Check response is 200 OK
	std::string s;
	std::stringstream res_str;

	res_str << echo_res;
	s = res_str.str();
	EXPECT_NE(s.find("200 OK"), std::string::npos);

	res_str << sleep_res;
	s = res_str.str();
	EXPECT_NE(s.find("200 OK"), std::string::npos);

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