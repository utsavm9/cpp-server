#include "session.h"

#include "echoHandler.h"
#include "gtest/gtest.h"

namespace http = boost::beast::http;

class MockRequestHandler : public RequestHandler {
   public:
	MockRequestHandler(http::response<http::string_body> response) : res_(response) {
	}

	bool can_handle(__attribute__((unused)) http::request<http::string_body> req) {
		return true;
	}

	http::response<http::string_body> handle_request(__attribute__((unused)) const http::request<http::string_body> &request) {
		return res_;
	}

   private:
	http::response<http::string_body> res_;
};

TEST(Session, ConstructResponse) {
	boost::asio::io_context io_context;
	auto config = std::make_shared<NginxConfig>();
	std::vector<std::pair<std::string, RequestHandler *>> url_to_handlers;
	tcp::socket socket(io_context);
	auto http_ok = http::response<http::string_body>{http::status::ok, 11};
	auto http_redirect = http::response<http::string_body>{http::status::permanent_redirect, 11};

	// create new session
	url_to_handlers.push_back({"/foo", new MockRequestHandler(http_ok)});
	url_to_handlers.push_back({"/foo/bar", new MockRequestHandler(http_redirect)});
	auto s = std::make_shared<session>(config.get(), url_to_handlers, std::move(socket));

	// test no exceptions are thrown in while starting session
	ASSERT_NO_THROW(s->start());

	{
		// Test normal response
		http::request<http::string_body> req{http::verb::get, "/foo", 11};
		http::response<http::string_body> res;
		s->construct_response(req, res);
		EXPECT_EQ(res.result(), http::status::ok);
	}

	{
		// Test longest prefix match
		http::request<http::string_body> req{http::verb::get, "/foo/bar/", 11};
		http::response<http::string_body> res;
		s->construct_response(req, res);
		EXPECT_EQ(res.result(), http::status::permanent_redirect);
	}

	{
		// Test unregistered
		http::request<http::string_body> req{http::verb::get, "/not_registered", 11};
		http::response<http::string_body> res;
		s->construct_response(req, res);
		EXPECT_EQ(res.result(), http::status::not_found);
	}

	{
		// Test that prefix match respects /
		// and does not match /foo to /foo_continued
		http::request<http::string_body> req{http::verb::get, "/foo_continued", 11};
		http::response<http::string_body> res;
		s->construct_response(req, res);
		EXPECT_EQ(res.result(), http::status::not_found);
	}

	{
		// Test that characters after the first "?" that is after the last "/" are ignored in URL
		http::request<http::string_body> req{http::verb::get, "/foo?fbclid=IwAR0-seXzN7KsoM2y0-LfD9H12NsKdqorpTjB1C4-0myBcO355Ax_wFTSouQ", 11};
		http::response<http::string_body> res;
		s->construct_response(req, res);
		EXPECT_EQ(res.result(), http::status::ok);
	}
}