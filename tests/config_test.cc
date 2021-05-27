#include "config.h"

#include <boost/filesystem.hpp>
#include <iostream>

#include "gtest/gtest.h"
#include "parser.h"

class NginxConfigTest : public ::testing::Test {
   protected:
	int default_port = NginxConfig::defaults["port"];
	NginxConfigParser parser_;

	void test_extract_field(const char *filename, int expected, std::string fieldname) {
		NginxConfig config;

		boost::filesystem::path filepath(filename);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		ASSERT_TRUE(parser_.Parse(filename, &config)) << "Parser error on file: " << filename;

		EXPECT_EQ(config.get_field(fieldname), expected) << "extracted wrong port number for: " << filepath;
	}
};

TEST_F(NginxConfigTest, GetField) {
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
		test_extract_field(p.first, p.second, "port");
	}

	test_extract_field("config/port_server_block", 0, "abcd");
}
