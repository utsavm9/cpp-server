#include "handler.h"

#include <boost/filesystem.hpp>
#include <unordered_map>

#include "echoHandler.h"
#include "fileHandler.h"
#include "gtest/gtest.h"
#include "notFoundHandler.h"
#include "parser.h"

namespace fs = boost::filesystem;

TEST(EchoHandlerTest, UnitTests) {
	{  // Check constructor
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("root \"./files\";  # supports relative path");
		p.Parse(&configStream, &config);
		EchoHandler esv("/echo", config);
		EXPECT_EQ("/echo", esv.get_url_prefix());
	}

	{
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("");
		p.Parse(&configStream, &config);
		EchoHandler esv("/echo", config);

		std::string s;
		http::request<http::string_body> req;
		req.method(http::verb::get);
		req.target("/echo");
		req.version(11);

		// Check OK response
		s = esv.to_string(esv.handle_request(req));
		EXPECT_NE(s.find("200 OK"), std::string::npos);
		EXPECT_NE(s.find("GET", 5), std::string::npos);
	}
}

TEST(NotFoundServiceTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("");
	p.Parse(&configStream, &config);
	NotFoundHandler esv("/", config);

	std::string s;
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.target("/");
	req.version(11);

	// Check OK response
	s = esv.to_string(esv.handle_request(req));
	EXPECT_NE(s.find("404 Not Found"), std::string::npos);
}

TEST(FileServiceTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("root \".\";");
	p.Parse(&configStream, &config);
	FileHandler fsv("/static", config);

	std::string s;
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.target("/static/file_that_doesn't_exist");
	req.version(11);

	// Check missing file
	s = fsv.to_string(fsv.handle_request(req));
	EXPECT_NE(s.find("404"), std::string::npos);

	// Test MIME
	std::unordered_map<std::string, std::string> checks{
	    {"index.html", "text/html"},
	    {"test.htm", "text/html"},
	    {"index.php", "text/html"},
	    {"index.css", "text/css"},
	    {"password.txt", "text/plain"},
	    {"malware.js", "application/javascript"},
	    {"geo.json", "application/json"},
	    {"data.xml", "application/xml"},
	    {"games.swf", "application/x-shockwave-flash"},
	    {"flying.flv", "video/x-flv"},
	    {"lossless.png", "image/png"},
	    {"me.jpg", "image/jpeg"},
	    {"me.jpeg", "image/jpeg"},
	    {"jff.gif", "image/gif"},
	    {"paint.bmp", "image/bmp"},
	    {"favi.ico", "image/vnd.microsoft.icon"},
	    {"why.tif", "image/tiff"},
	    {"what.tiff", "image/tiff"},
	    {"sun.svg", "image/svg+xml"},
	    {"earth.svgz", "image/svg+xml"},
	    {"hw.zip", "application/zip"},
	    {"extensionless", "text/plain"},
	    {"/", "text/plain"},
	};

	for (auto& p : checks) {
		EXPECT_EQ(fsv.get_mime(p.first), p.second);
	}
}

class FileHandlerTest : public ::testing::Test {
   public:
	// Creates the file at current directory.
	void test_file_content(std::string url_prefix, std::string location_root, std::string filename, std::string target) {
		// Create a file and ensure it exists
		std::ofstream testfile(filename);
		ASSERT_TRUE(testfile) << "could not create a test file, aborting test";
		testfile << "sample string" << std::endl;
		testfile.close();
		ASSERT_TRUE(fs::exists(filename)) << "test file does not exist even though it was just created";

		// File handler to serve files from current directory
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("root " + location_root + ";");
		p.Parse(&configStream, &config);
		FileHandler fsv(url_prefix, config);

		// Construct request
		http::request<http::string_body> req;
		req.method(http::verb::get);
		req.version(11);
		req.target(target);

		// Check if file handler can get the file
		std::string res = fsv.to_string(fsv.handle_request(req));
		EXPECT_NE(res.find("sample string"), std::string::npos) << "requested file was not served, response was: " << res;

		// Clean up
		ASSERT_TRUE(fs::remove(filename)) << "test file got deleted already unexpectedly";
	}

	void test_not_ok(std::string url_prefix, std::string location_root, std::string target) {
		// File handler to serve files from current directory
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("root " + location_root + ";");
		p.Parse(&configStream, &config);
		FileHandler fsv(url_prefix, config);

		// Construct request
		http::request<http::string_body> req;
		req.method(http::verb::get);
		req.version(11);
		req.target(target);

		// Check if file handler can get the file
		std::string res = fsv.to_string(fsv.handle_request(req));
		EXPECT_EQ(res.find("200 OK"), std::string::npos) << "unexpectedly got a OK response, response was: " << res;
	}
};

TEST_F(FileHandlerTest, UnitTests) {
	{
		// Check constructor
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("root \"./files\";  # supports relative path");
		p.Parse(&configStream, &config);
		FileHandler fsv("/static", config);
		EXPECT_EQ("./files", fsv.get_linux_dir());
	}

	{  // Check constructor does not throw exceptions on bad config
		NginxConfig config;
		NginxConfigParser p;
		std::istringstream configStream;

		configStream.str("invalid;");
		p.Parse(&configStream, &config);
		ASSERT_NO_THROW({
			FileHandler f("/file", config);
			http::request<http::string_body> req;
			std::string res = f.to_string(f.handle_request(req));
		});
	}

	{
		// Check that trailing slashes are ignored
		test_file_content("/static", ".", "testfile.txt", "/static/testfile.txt");
		test_file_content("/static/", ".", "testfile.txt", "/static/testfile.txt");
		test_file_content("/static/", "./", "testfile.txt", "/static/testfile.txt");
		test_file_content("/static/", "./", "testfile.txt", "/static/testfile.txt/");

		// Check that files can be extracted from repos
		test_file_content("/static/", "./testfile.txt", "testfile.txt", "/static");

		// Check that invalid settings do not return a 200 OK
		test_not_ok("/static", "nonexistant-repo", "/static/");
	}
}
