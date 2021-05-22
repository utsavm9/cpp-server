#include "logger.h"

#include <boost/filesystem.hpp>
#include <ctime>
#include <fstream>

#include "gtest/gtest.h"

TEST(LogTests, LogsProperly) {
	system("/bin/bash ./logger_test_scripts/delete_logs.sh");
	init_logger();
	std::string log_msg = "Start of first log";
	const int log_msg_size = log_msg.size();
	TRACE << log_msg;
	time_t t = time(NULL);
	tm* timePtr = localtime(&t);
	std::string month;
	std::string day;
	if (timePtr->tm_mon < 9) {  // if month is single digit (tm_mon ranges from 0 to 11)
		month = "0" + std::to_string(timePtr->tm_mon + 1);
	} else {
		month = std::to_string(timePtr->tm_mon + 1);
	}
	if (timePtr->tm_mday < 10) {
		day = "0" + std::to_string(timePtr->tm_mday);
	} else {
		day = std::to_string(timePtr->tm_mday);
	}
	std::string log_file = "./logs/SERVER_LOG_" + std::to_string(timePtr->tm_year + 1900) + "-" + month + "-" + day + "_0.log";
	std::cout << log_file;
	std::ifstream ifs(log_file);
	// read content of log file and put it into a string
	std::string log_file_content((std::istreambuf_iterator<char>(ifs)),
	                             (std::istreambuf_iterator<char>()));
	const int log_file_msg_starting_pos = 73;
	int log_works = log_file_content.compare(log_file_msg_starting_pos, log_msg_size, log_msg);
	system("/bin/bash ./logger_test_scripts/delete_logs.sh");
	EXPECT_EQ(log_works, 0);
}