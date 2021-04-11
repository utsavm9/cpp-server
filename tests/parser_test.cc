#include <boost/filesystem.hpp>

#include "config_parser.h"
#include "gtest/gtest.h"

TEST(NginxConfigParserEdgeCases, NullConfig) {
	NginxConfigParser parser;
	NginxConfig out_config;
	std::istream *nullStream = nullptr;

	ASSERT_EXIT(
	    {
		    parser.Parse("example_config", nullptr);
		    std::cerr << "did not crash";
		    exit(0);
	    },
	    ::testing::ExitedWithCode(0), "did not crash");

	ASSERT_EXIT(
	    {
		    parser.Parse(nullStream, &out_config);
		    std::cerr << "did not crash";
		    exit(0);
	    },
	    ::testing::ExitedWithCode(0), "did not crash");

	ASSERT_EXIT(
	    {
		    parser.Parse(nullStream, nullptr);
		    std::cerr << "did not crash";
		    exit(0);
	    },
	    ::testing::ExitedWithCode(0), "did not crash");
}

class NginxConfigParserTest : public ::testing::Test {
   protected:
	NginxConfigParser parser_;

	void config_expect_fail(const char *filename) {
		NginxConfig out_config_;
		EXPECT_FALSE(parser_.Parse(filename, &out_config_)) << "Did not error on invalid file: " << filename;
	}

	void config_expect_pass(const char *filename) {
		NginxConfig out_config_;
		EXPECT_TRUE(parser_.Parse(filename, &out_config_)) << "Errored on correct file: " << filename;
	}
};

TEST_F(NginxConfigParserTest, ValidConfigs) {
	const char *files[] = {
	    "parser/example_config",
	    "parser/multiple_blocks_config",
	    "parser/empty_block_config",
	    "parser/quotes_config",
	    "parser/escape_char_config",
	};

	for (auto file : files) {
		boost::filesystem::path filepath(file);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;

		config_expect_pass(file);
	}
}

TEST_F(NginxConfigParserTest, InvalidConfigs) {
	config_expect_fail("non-existant-file");

	const char *files[] = {
	    "parser/invalid_config",
	    "parser/extra_closing_braces_config",
	    "parser/missing_opening_braces_config",
	    "parser/invalid_escape_config",
	    "parser/missing_space_after_quotes_config",
	};

	for (auto file : files) {
		boost::filesystem::path filepath(file);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;

		config_expect_fail(file);
	}
}
