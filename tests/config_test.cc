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

TEST_F(NginxConfigTest, ExtractPort) {
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

TEST_F(NginxConfigTest, ParseArgs) {
	ASSERT_DEATH(
	    {
		    NginxConfig config;
		    const char *argv[] = {(char *)("program-name")};
		    NginxConfigParser::parse_args(1, argv, &config);
	    },
	    "");

	ASSERT_DEATH(
	    {
		    const char *argv[2];
		    argv[0] = (char *)("program-name");
		    argv[1] = (char *)("file_name");
		    NginxConfigParser::parse_args(2, argv, nullptr);
	    },
	    "");

	{
		NginxConfig config;
		const char *argv_good_config[] = {
		    (char *)("program-name"),
		    (char *)("config/port_top_level")};

		boost::filesystem::path filepath(argv_good_config[1]);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		NginxConfigParser::parse_args(2, argv_good_config, &config);
		EXPECT_EQ(config.get_port(), 100);
	}

	{
		NginxConfig config;
		const char *argv_bad_config[] = {
		    (char *)("program-name"),
		    (char *)("parser/missing_opening_braces_config")};

		boost::filesystem::path filepath(argv_bad_config[1]);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		NginxConfigParser::parse_args(2, argv_bad_config, &config);
		EXPECT_EQ(config.get_port(), 80);
	}
}

TEST_F(NginxConfigTest, ExtractTargets) {
	NginxConfig config;
	std::string filename;
	boost::filesystem::path filepath;

	// Valid config
	filename = "config/find_paths";
	filepath = boost::filesystem::path(filename);
	ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
	ASSERT_TRUE(parser_.Parse("config/find_paths", &config)) << "Parser error on file: " << filename;

	config.extract_targets();

	EXPECT_EQ(config.urlToServiceName[0].second, "static");
	EXPECT_EQ(config.urlToServiceName[1].second, "static");
	EXPECT_EQ(config.urlToServiceName[2].second, "echo");
	EXPECT_EQ(config.urlToServiceName[3].second, "echo");
	EXPECT_EQ(config.urlToLinux["/static"], "/static_data");
	EXPECT_EQ(config.urlToLinux["/file"], "/static_data_1");
	config.free_memory();

	filename = "config/unknown_target";
	filepath = boost::filesystem::path(filename);
	ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
	ASSERT_TRUE(parser_.Parse("config/unknown_target", &config)) << "Parser error on file: " << filename;

	config.urlToServiceName = std::vector<std::pair<std::string, std::string>>{};
	config.urlToLinux = std::unordered_map<std::string, std::string>{};
	config.extract_targets();

	EXPECT_EQ(config.urlToServiceName[0].second, "static");
	EXPECT_EQ(config.urlToServiceName[1].second, "static");
	EXPECT_EQ(config.urlToServiceName[2].second, "echo");
	EXPECT_EQ(config.urlToServiceName[3].second, "echo");
	EXPECT_EQ(config.urlToLinux["/static"], "/static_data");
	EXPECT_EQ(config.urlToLinux["/file"], "/static_data_1");
	config.free_memory();

	// Invalid config

	std::vector<std::string> filenames_err =
	    {"config/multiple_static_paths_err",
	     "config/no_fs_mapping_err",
	     "config/echo_multiple_tokens_err"};

	for (auto filename : filenames_err) {
		filepath = boost::filesystem::path(filename);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		ASSERT_TRUE(parser_.Parse(filename.c_str(), &config)) << "Parser error on file: " << filename;

		config.urlToServiceName = std::vector<std::pair<std::string, std::string>>{};
		config.urlToLinux = std::unordered_map<std::string, std::string>{};
		config.extract_targets();

		EXPECT_EQ(config.urlToLinux.size(), 0);
		EXPECT_EQ(config.urlToServiceName.size(), 0);
		config.free_memory();
	}
}