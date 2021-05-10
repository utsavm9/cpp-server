#include "server.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "config.h"
#include "gtest/gtest.h"

void server_runner(boost::asio::io_context* io_context, NginxConfig config, bool* done) {
	server::serve_forever(io_context, config);
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
	    "port 8080; # The port my server listens on\n"
	    "location \"/echo\" EchoHandler {}\n"
	    "location \"/print\" EchoHandler {}\n"
	    "location \"/static\" StaticHandler {\n"
	    "root \"../data/static_data\";\n"
	    "}\n"
	    "location \"/\" 404Handler{}\n"
	    "location \"/notvalid\" ErrorButServerShouldntCrash{}\n");
	p.Parse(&configStream, &config);
	ASSERT_EQ(config.get_port(), 8080);
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