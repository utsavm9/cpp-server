#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "echoHandler.h"
#include "fileHandler.h"
#include "gtest/gtest.h"
#include "healthHandler.h"
#include "logger.h"
#include "notFoundHandler.h"
#include "parser.h"
#include "proxyRequestHandler.h"
#include "server.h"
#include "statusHandler.h"

namespace fs = boost::filesystem;

class TestProxyRequestHandler : public ProxyRequestHandler {
   private:
	boost::beast::http::response<boost::beast::http::string_body> m_res;
	boost::beast::http::response<boost::beast::http::string_body> req_synchronous(const boost::beast::http::request<boost::beast::http::string_body> &) {
		return m_res;
	}

   public:
	TestProxyRequestHandler(const std::string &url_prefix, const NginxConfig &config) : ProxyRequestHandler{url_prefix, config} {}
	void set_fake_response(const boost::beast::http::response<boost::beast::http::string_body> &http_response) {
		m_res = http_response;
	}
};

TEST(Handler, StaticUnitTests) {
	EXPECT_EQ(RequestHandler::bad_request().result(), http::status::bad_request);
	EXPECT_EQ(RequestHandler::not_found_error().result(), http::status::not_found);
	EXPECT_EQ(RequestHandler::internal_server_error().result(), http::status::internal_server_error);
}

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
		s = esv.to_string(esv.get_response(req));
		EXPECT_NE(s.find("200 OK"), std::string::npos);
		EXPECT_NE(s.find("GET", 5), std::string::npos);
	}
}

class FileHandlerTest : public ::testing::Test {
   public:
	// Creates the file at current directory.
	void test_file_content(std::string url_prefix, std::string location_root, std::string filename, std::string target, std::string mime) {
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
		std::string res = fsv.to_string(fsv.get_response(req));
		EXPECT_NE(res.find("sample string"), std::string::npos) << "requested file was not served, response was: " << res;
		EXPECT_NE(res.find(mime), std::string::npos) << "wrong mime type of file, response was: " << res;

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
		std::string res = fsv.to_string(fsv.get_response(req));
		EXPECT_EQ(res.find("200 OK"), std::string::npos) << "unexpectedly got a OK response, response was: " << res;
	}
};

TEST_F(FileHandlerTest, UnitTests) {
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
	s = fsv.to_string(fsv.get_response(req));
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

TEST_F(FileHandlerTest, FileTests) {
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
			std::string res = f.to_string(f.get_response(req));
		});
	}

	{
		// Check that trailing slashes are ignored
		test_file_content("/static", ".", "testfile.txt", "/static/testfile.txt", "text/plain");
		test_file_content("/static/", ".", "testfile.txt", "/static/testfile.txt", "text/plain");
		test_file_content("/static/", "./", "testfile.txt", "/static/testfile.txt", "text/plain");
		test_file_content("/static/", "./", "testfile.txt", "/static/testfile.txt/", "text/plain");

		// Check that files can be extracted from repos
		test_file_content("/static/", "./testfile.txt", "testfile.txt", "/static", "text/plain");

		// Check correct MIME type
		test_file_content("/static/", "./image.jpg", "image.jpg", "/static", "image/jpeg");
		test_file_content("/static/", "./image.jpeg", "image.jpeg", "/static", "image/jpeg");
		test_file_content("/static/", "./hw.zip", "hw.zip", "/static", "application/zip");

		// Check that invalid settings do not return a 200 OK
		test_not_ok("/static", "nonexistant-repo", "/static/");
	}
}

TEST(NotFoundHandlerTest, UnitTests) {
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
	s = esv.to_string(esv.get_response(req));
	EXPECT_NE(s.find("404 Not Found"), std::string::npos);
}

class ProxyHandlerTest : public ::testing::Test {
   protected:
	TestProxyRequestHandler *handler;
	TestProxyRequestHandler *root_handler;
	NginxConfig config;
	NginxConfigParser p;
	std::string proxy_path = "/proxy";
	http::request<http::string_body> handler_request;
	http::request<http::string_body> root_handler_request;
	void SetUp() override {
		std::istringstream config_stream;
		config_stream.str("dest \"www.test.test\"; port 80;");
		p.Parse(&config_stream, &config);
		handler = new TestProxyRequestHandler(proxy_path, config);
		root_handler = new TestProxyRequestHandler("/", config);
		handler_request.method(http::verb::get);
		handler_request.target(proxy_path);
		handler_request.version(11);
		root_handler_request.method(http::verb::get);
		root_handler_request.target("/");
		root_handler_request.version(11);
	}
	void TearDown() override {
		delete handler;
		delete root_handler;
	}

	bool proxy_handler_contains(const http::response<http::string_body> &fake_res,
	                            const std::string &expected) {
		handler->set_fake_response(fake_res);
		std::string handler_response = handler->to_string(handler->get_response(handler_request));
		return handler_response.find(expected) != std::string::npos;
	}

	bool root_proxy_handler_contains(const http::response<http::string_body> &fake_res,
	                                 const std::string &expected) {
		root_handler->set_fake_response(fake_res);
		std::string root_handler_response = root_handler->to_string(root_handler->get_response(root_handler_request));
		return root_handler_response.find(expected) != std::string::npos;
	}
};

TEST_F(ProxyHandlerTest, UnitTests) {
	// Create fake responses
	std::string html_body = "href=\"/asdf\"\nsrc=\"//qwer\"";

	http::response<http::string_body> fake_res;
	fake_res.result(200);
	fake_res.set(http::field::content_type, "text/html");
	fake_res.body() = html_body;
	fake_res.prepare_payload();

	http::response<http::string_body> non_html_res(fake_res);
	non_html_res.set(http::field::content_type, "text/plain");

	// Test proxy handler correctly proxies and updates appropriate relative html links
	EXPECT_TRUE(proxy_handler_contains(fake_res, "href=\"" + proxy_path + "/asdf\""));
	EXPECT_TRUE(proxy_handler_contains(fake_res, "src=\"//qwer\""));
	EXPECT_TRUE(proxy_handler_contains(fake_res, "200 OK"));

	// Test proxy handler mapped to root correctly proxies and
	// relative links should not change
	EXPECT_TRUE(root_proxy_handler_contains(fake_res, html_body));
	EXPECT_TRUE(root_proxy_handler_contains(fake_res, "200 OK"));

	// Test proxy handler does not modify links if non-html
	EXPECT_TRUE(proxy_handler_contains(non_html_res, non_html_res.body()));
	EXPECT_TRUE(proxy_handler_contains(non_html_res, "200 OK"));
}

TEST_F(ProxyHandlerTest, RedirectUnitTests) {
	std::string absolute_path = "https://www.smth.com/asdf";
	std::string relative_path = "/asdf";
	http::response<http::string_body> fake_res_absolute;
	fake_res_absolute.result(301);
	fake_res_absolute.set(http::field::location, absolute_path);
	fake_res_absolute.prepare_payload();

	http::response<http::string_body> fake_res_relative(fake_res_absolute);
	fake_res_relative.result(302);
	fake_res_relative.set(http::field::location, relative_path);

	// Test 301 redirects are appropriately returned and absolute paths do not change
	EXPECT_TRUE(proxy_handler_contains(fake_res_absolute, "Location: " + absolute_path));
	EXPECT_TRUE(proxy_handler_contains(fake_res_absolute, "301 Moved Permanently"));

	// Test 302 redirects are appropriately returned and relative paths udpated for proxy
	EXPECT_TRUE(proxy_handler_contains(fake_res_relative, proxy_path + relative_path));
	EXPECT_TRUE(proxy_handler_contains(fake_res_relative, "302 Found"));

	// Test 302 redirects are appropriately returned and relative paths do not change if
	// proxy handler is mapped to root
	EXPECT_TRUE(root_proxy_handler_contains(fake_res_relative, relative_path));
	EXPECT_TRUE(root_proxy_handler_contains(fake_res_relative, "302 Found"));
}

TEST(StatusHandlerTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("");
	p.Parse(&configStream, &config);
	StatusHandler test_handler("/", config);

	StatusHandler::url_to_res_code.push_back({"/foo", "200"});
	StatusHandler::url_to_res_code.push_back({"/brick", "404"});
	StatusHandler::url_to_res_code.push_back({"/file/trash.txt", "200"});

	server::urlToHandlerName.push_back({"/foo", "FooHandler"});
	server::urlToHandlerName.push_back({"/", "NotFoundHandler"});
	server::urlToHandlerName.push_back({"/file", "FileHandler"});
	server::urlToHandlerName.push_back({"/status", "StatusHandler"});

	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);
	req.target("/status");
	std::string res = test_handler.to_string(test_handler.get_response(req));

	//test request to response code table is set correctly
	EXPECT_NE(res.find("Total Requests Received: 4"), std::string::npos);
	EXPECT_NE(res.find("/foo</td><td>200"), std::string::npos);
	EXPECT_NE(res.find("/brick</td><td>404"), std::string::npos);
	EXPECT_NE(res.find("/file/trash.txt</td><td>200"), std::string::npos);
	EXPECT_NE(res.find("/status</td><td>200"), std::string::npos);

	//test url prefix to handler table is set correctly
	EXPECT_NE(res.find("/foo</td><td>FooHandler"), std::string::npos);
	EXPECT_NE(res.find("/</td><td>NotFoundHandler"), std::string::npos);
	EXPECT_NE(res.find("/file</td><td>FileHandler"), std::string::npos);
	EXPECT_NE(res.find("/status</td><td>StatusHandler"), std::string::npos);

	std::pair<std::string, std::string> top = StatusHandler::url_to_res_code[StatusHandler::url_to_res_code.size() - 1];

	//check get_response adds url and response code pair
	ASSERT_TRUE(top.first == "/status");
	ASSERT_TRUE(top.second == "200");

	StatusHandler::url_to_res_code.clear();
	server::urlToHandlerName.clear();
}

TEST(HealthHandlerTest, UnitTests) {
	NginxConfig config;
	NginxConfigParser p;
	std::istringstream configStream;

	configStream.str("");
	p.Parse(&configStream, &config);
	HealthHandler test_handler("/", config);

	http::request<http::string_body> req;
	req.method(http::verb::get);
	req.version(11);
	req.target("/health");
	std::string res = test_handler.to_string(test_handler.get_response(req));

	EXPECT_NE(res.find("200 OK"), std::string::npos);
}