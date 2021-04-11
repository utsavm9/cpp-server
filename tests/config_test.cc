#include <boost/filesystem.hpp>

#include "config_parser.h"
#include "gtest/gtest.h"

class NginxConfigTest : public ::testing::Test {
   protected:
	int default_port = 80;
	NginxConfigParser parser_;

	void test_find_port(const char* filename, int expected) {
		NginxConfig config;

		boost::filesystem::path filepath(filename);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		ASSERT_TRUE(parser_.Parse(filename, &config)) << "Parser error on file: " << filename;

		EXPECT_EQ(config.find_port(), expected);
	}
};

TEST_F(NginxConfigTest, FindPort) {
	std::pair<const char*, int> file_ports[]{
	    {"config/exemplar", 100},
	    {"config/missing_listen", default_port},
	    {"config/missing_port", default_port},
	    {"config/missing_server", default_port},
	    {"config/non_numeric_port", default_port},
	    {"config/wrong_listen_format", default_port},
	};

	for (auto& p : file_ports) {
		test_find_port(p.first, p.second);
	}
}