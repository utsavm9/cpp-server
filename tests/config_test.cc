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
	    {"config/too_long_port", default_port},
	    {"config/wrong_listen_format", default_port},
	};

	for (auto& p : file_ports) {
		test_find_port(p.first, p.second);
	}
}

TEST_F(NginxConfigTest, FindPaths) {
	NginxConfig config;
	auto filename = "config/find_paths";
	boost::filesystem::path filepath(filename);
	ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
	ASSERT_TRUE(parser_.Parse(filename, &config)) << "Parser error on file: " << filename;

	std::unordered_map<std::string, std::vector<std::string>> correct_map;
	std::vector<std::string> static_vec{"/static", "/file"};
	std::vector<std::string> echo_vec{"/echo", "/print"};

	correct_map.insert(std::make_pair("static", static_vec));
	correct_map.insert(std::make_pair("echo", echo_vec));

	auto test_map = config.find_paths();

	for (auto element : test_map){
		ASSERT_TRUE(test_map["static"] == correct_map["static"]) << "Static paths incorrect!";
		ASSERT_TRUE(test_map["echo"] == correct_map["echo"]) << "Echo paths incorrect!";
	}
	
}