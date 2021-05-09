#include <boost/filesystem.hpp>
#include <unordered_map>

#include "echoService.h"
#include "fileService.h"
#include "gtest/gtest.h"
#include "parser.h"
#include "service.h"

namespace fs = boost::filesystem;

/*TEST(ServiceTest, UnitTests) {
	Service sv;
	http::request<http::string_body> req;
	http::response<http::string_body> res;

	// Ensure cannot handle requests
	EXPECT_FALSE(sv.can_handle(req));

	// Ensure nothing is servered by the default service
	std::string s = sv.make_response(req);
	EXPECT_EQ(s.find("OK"), std::string::npos);

	// Check other HTTP error responses
	s = sv.bad_request();
	EXPECT_NE(s.find("400 Bad Request"), std::string::npos);

	s = sv.not_found_error();
	EXPECT_NE(s.find("404 Not Found"), std::string::npos);
}*/

TEST(EchoServiceTest, UnitTests) {
	std::string s;
	EchoService esv("/echo");
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);

	// Testing if difficult targets are correctly idenfied
	// as the ones being handled or not
	req.target("");
	EXPECT_FALSE(esv.can_handle(req));
	req.target("/other");
	EXPECT_FALSE(esv.can_handle(req));
	req.target("/echo");
	EXPECT_TRUE(esv.can_handle(req));

	// Check OK response
	s = esv.to_string(esv.handle_request(req));
	EXPECT_NE(s.find("200 OK"), std::string::npos);
	EXPECT_NE(s.find("GET", 5), std::string::npos);
}

TEST(FileServiceTest, UnitTests) {
	FileService fsv("/static", ".");
	std::string s;
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);

	// Testing if difficult targets are correctly idenfied
	// as the ones being handled or not
	req.target("");
	EXPECT_FALSE(fsv.can_handle(req));
	req.target("static/");
	EXPECT_FALSE(fsv.can_handle(req));
	req.target("/other");
	EXPECT_FALSE(fsv.can_handle(req));
	req.target("/..");
	EXPECT_FALSE(fsv.can_handle(req));
	req.target("/static/");
	EXPECT_TRUE(fsv.can_handle(req));

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

	// File service to serve files from current directory
	FileService fsv("/static", ".");

	// Construct request
	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);
	req.target("/static/test.txt");

	// Check if file service can get the file
	EXPECT_TRUE(fsv.can_handle(req)) << "file service cannot handle legitimate request";
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
	FileService fsv("/static", config);
	EXPECT_EQ("./files", fsv.get_linux_dir());
}

TEST(EchoServiceTest, NewConstructor) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("root \"./files\";  # supports relative path");
	p.Parse(&configStream, &config);
	EchoService esv("/echo", config);
	EXPECT_EQ("/echo", esv.get_url_prefix());
}