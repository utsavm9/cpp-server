#include "config.h"

#include <boost/filesystem.hpp>
#include <iostream>

#include "gtest/gtest.h"
#include "parser.h"

class NginxConfigTest : public ::testing::Test {
   protected:
	int unchanged = -100;
	NginxConfigParser parser_;

	void test_extract_port(const char *filename, int expected) {
		NginxConfig config;

		boost::filesystem::path filepath(filename);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;
		ASSERT_TRUE(parser_.Parse(filename, &config)) << "Parser error on file: " << filename;

		config.port = unchanged;
		config.extract_port();
		EXPECT_EQ(config.port, expected);
	}
};

TEST_F(NginxConfigTest, ExtractPort) {
	std::pair<const char *, int> file_ports[]{
	    {"config/exemplar", 100},
	    {"config/missing_listen", unchanged},
	    {"config/missing_port", unchanged},
	    {"config/missing_server", unchanged},
	    {"config/non_numeric_port", unchanged},
	    {"config/too_long_port", unchanged},
	    {"config/wrong_listen_format", unchanged},
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
		    const char *argv[] = {(char *)("program-name", "conf-file")};
		    NginxConfigParser::parse_args(1, argv, nullptr);
	    },
	    "");

	{
		NginxConfig config;
		const char *argv_good_config[] = {
		    (char *)("program-name"),
		    (char *)("config/exemplar")};
		NginxConfigParser::parse_args(2, argv_good_config, &config);
		EXPECT_EQ(config.port, 100);
	}

	{
		NginxConfig config;
		config.port = 99;
		const char *argv_bad_config[] = {
		    (char *)("program-name"),
		    (char *)("parser/missing_opening_braces_config")};
		NginxConfigParser::parse_args(2, argv_bad_config, &config);
		EXPECT_EQ(config.port, 99);
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