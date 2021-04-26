#include "server.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "gtest/gtest.h"

void server_runner(boost::asio::io_context* io_context, int port, bool* done) {
	std::unordered_map<std::string, std::vector<std::string>>* dummy_map;
	server::serve_forever(io_context, port, dummy_map);
	*done = true;
}

TEST(ServerTest, ServeForever) {
	// Spawn the server
	boost::asio::io_context io_context;
	bool done = false;
	std::thread server_thread(server_runner, &io_context, 8080, &done);

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