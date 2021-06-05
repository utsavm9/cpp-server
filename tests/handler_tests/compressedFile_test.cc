#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <streambuf>
#include <string>
#include <unordered_map>

#include "compressedFileHandler.h"
#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"
#include "server.h"

TEST(CompressedFileHandlerTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("root \"../tests/handler_tests\";");
	p.Parse(&configStream, &config);

	CompressedFileHandler cf_handler("/compressed", config);

	std::ifstream t("../tests/handler_tests/hello.txt");
	std::string hello_str((std::istreambuf_iterator<char>(t)),
	                      std::istreambuf_iterator<char>());

	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.target("/compressed/hello.txt");
	req.set(http::field::accept_encoding, "gzip");
	req.version(11);

	http::response<http::string_body> res = cf_handler.get_response(req);

	http::request<http::string_body> reqa;
	reqa.method(http::verb::get);
	reqa.target("/compressed/hello.txt");
	reqa.version(11);

	// no accept_encoding, so body is not compressed
	http::response<http::string_body> resa = cf_handler.get_response(reqa);

	// compressed < uncompressed
	EXPECT_LT(res.body().size(), hello_str.size());

	// uncompressed vs uncompressed
	EXPECT_EQ(resa.body().size(), hello_str.size());
}