#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"
#include "server.h"
#include "proxyRequestHandler.h"

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