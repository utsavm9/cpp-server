#include "parser.h"

#include <boost/filesystem.hpp>

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
	    "parser/escape_single_quotes",
	    "parser/comment_config",
	    "parser/multi_comment_config"};

	for (auto file : files) {
		boost::filesystem::path filepath(file);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;

		config_expect_pass(file);
	}
}

TEST_F(NginxConfigParserTest, InvalidConfigs) {
	config_expect_fail("non-existant-file");

	const char *files[] = {
	    "parser/missing_statement_end",
	    "parser/extra_closing_braces_config",
	    "parser/missing_opening_braces_config",
	    "parser/invalid_escape_config",
	    "parser/missing_end_block",
	    "parser/missing_space_after_quotes_config",
	    "parser/missing_space_after_doublequotes",
	    "parser/invalid_single_quote_escape",
	    "parser/invalid_double_quote_escape",
	};

	for (auto file : files) {
		boost::filesystem::path filepath(file);
		ASSERT_TRUE(boost::filesystem::exists(filepath)) << "File does not exist: " << filepath;

		config_expect_fail(file);
	}
}

TEST_F(NginxConfigParserTest, EscapeQuotes) {
	NginxConfig out_config_;
	std::string parsed_string;
	const char *filename = "parser/check_quoted_string";

	boost::filesystem::path filepath(filename);
	ASSERT_TRUE(boost::filesystem::exists(filepath)) << "file does not exist: " << filepath;
	ASSERT_TRUE(parser_.Parse(filename, &out_config_)) << "unable to parse a vaild config: " << filepath;

	ASSERT_NO_THROW(parsed_string = out_config_.statements_[0]->tokens_[0]) << "config has wrong structure, parsed from: " << filepath;
	ASSERT_EQ(parsed_string, "string with \" escaped quotes");
}