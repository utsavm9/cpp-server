#include "config.h"

#include <boost/filesystem.hpp>
#include <iostream>

#include "gtest/gtest.h"
#include "parser.h"

class NginxConfigTest : public ::testing::Test {
   protected:
	int default_port = 80;
	NginxConfigParser parser_;

	void test_extract_port(const char *filename, int expected) {
		NginxConfig config;

		boost::filesystem::path filepath(filename);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		ASSERT_TRUE(parser_.Parse(filename, &config)) << "Parser error on file: " << filename;

		EXPECT_EQ(config.get_port(), expected) << "extracted wrong port number for: " << filepath;
	}
};

TEST_F(NginxConfigTest, GetPort) {
	std::pair<const char *, int> file_ports[]{
	    {"config/port_server_block", 100},
	    {"config/port_top_level", 100},
	    {"config/missing_listen", default_port},
	    {"config/missing_port", default_port},
	    {"config/missing_server", default_port},
	    {"config/non_numeric_port", default_port},
	    {"config/too_long_port", default_port},
	    {"config/wrong_listen_format", default_port},
	};

	for (auto &p : file_ports) {
		test_extract_port(p.first, p.second);
	}
}
