#include "session.h"

#include "echoService.h"
#include "gtest/gtest.h"

namespace http = boost::beast::http;

class MockService : public Service {
   public:
	MockService(std::string response) : response(response) {
	}

	std::string make_response(__attribute__((unused)) http::request<http::string_body> req) {
		return response;
	}

	bool can_handle(__attribute__((unused)) http::request<http::string_body> req) {
		return true;
	}

	http::response<http::string_body> handle_request(__attribute__((unused)) const http::request<http::string_body> &request) {
		http::response<http::string_body> res;
		return res;
	}

   private:
	std::string response;
};

TEST(Session, ConstructResponse) {
	boost::asio::io_context io_context;
	NginxConfig *config = new NginxConfig();
	std::vector<std::pair<std::string, Service *>> url_to_handlers;
	int max_len = 1024;
	std::string http_ok = "200 OK";
	std::string http_bad_request = "400 Bad Request";

	// create new session
	url_to_handlers.push_back(std::make_pair("/foo", new MockService(http_ok)));
	url_to_handlers.push_back(std::make_pair("/foo/bar", new MockService(http_ok)));
	session new_session(io_context, config, url_to_handlers, max_len);

	// test max_len
	std::string max_len_test = new_session.construct_response(max_len + 1);
	auto pos = max_len_test.find(http_ok);
	EXPECT_EQ(pos, std::string::npos);

	// test bad req_str
	new_session.change_data("foo");
	std::string bad_req_str_test = new_session.construct_response(max_len);
	pos = bad_req_str_test.find(http_ok);
	EXPECT_EQ(pos, std::string::npos);

	// test normal response
	new_session.change_data("GET /foo HTTP/1.1\r\n\r\n");
	std::string normal_response_test = new_session.construct_response(max_len);
	pos = normal_response_test.find(http_ok);
	EXPECT_EQ(pos, 0);

	// test longest prefix match
	new_session.change_data("GET /foo/bar HTTP/1.1\r\n\r\n");
	normal_response_test = new_session.construct_response(max_len);
	pos = normal_response_test.find(http_ok);
	EXPECT_EQ(pos, 0);

	// test bad request (no service handler)
	url_to_handlers.clear();
	new_session.change_data("GET /echo HTTP/1.1\r\n\r\n");
	std::string bad_req_test = new_session.construct_response(max_len);
	pos = bad_req_test.find(http_ok);
	EXPECT_EQ(pos, std::string::npos);
}