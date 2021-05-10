#include <boost/filesystem.hpp>
#include <unordered_map>

#include "echoHandler.h"
#include "fileHandler.h"
#include "gtest/gtest.h"
#include "notfoundHandler.h"
#include "parser.h"
#include "requestHandler.h"

namespace fs = boost::filesystem;

TEST(EchoHandlerTest, UnitTests) {
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

	for (auto &p : checks) {
		EXPECT_EQ(fsv.get_mime(p.first), p.second);
	}
}

TEST(FileServiceTest, ReadFile) {
	// Create a file and ensure it exists
	std::ofstream testfile("test.txt");
	ASSERT_TRUE(testfile) << "could not create a test file, aborting test";
	testfile << "sample string" << std::endl;
	testfile.close();
	ASSERT_TRUE(fs::exists("test.txt")) << "test file does not exist even though it was just created";

	// File handler to serve files from current directory
	FileHandler fsv("/static", ".");

	// Construct request
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);
	req.target("/static/test.txt");

	// Check if file handler can get the file
	std::string res = fsv.to_string(fsv.handle_request(req));
	EXPECT_NE(res.find("sample string"), std::string::npos) << "requested file was not served, response was: " << res;

	// Clean up
	ASSERT_TRUE(fs::remove("test.txt")) << "test file got deleted already unexpectedly";
}

TEST(FileServiceTest, NewConstructor) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("root \"./files\";  # supports relative path");
	p.Parse(&configStream, &config);
	FileHandler fsv("/static", config);
	EXPECT_EQ("./files", fsv.get_linux_dir());
}

TEST(EchoHandlerTest, NewConstructor) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("root \"./files\";  # supports relative path");
	p.Parse(&configStream, &config);
	EchoHandler esv("/echo", config);
	EXPECT_EQ("/echo", esv.get_url_prefix());
}