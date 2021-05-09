#include "session.h"

#include "echoService.h"
#include "gtest/gtest.h"

namespace http = boost::beast::http;

class MockService : public Service {
   public:
	std::string make_response(__attribute__((unused)) http::request<http::string_body> req) {
		return "200 OK";
	}

	bool can_handle(__attribute__((unused)) http::request<http::string_body> req) {
		return true;
	}

	http::response<http::string_body> handle_request(__attribute__((unused)) const http::request<http::string_body> &request) {
		http::response<http::string_body> res;
		return res;
	}
};

TEST(Session, ConstructResponse) {
	boost::asio::io_context io_context;
	NginxConfig *config = new NginxConfig();
	std::vector<Service *> service_handlers;
	int max_len = 1024;
	std::string http_ok = "200 OK";

	// create new session
	service_handlers.push_back(new MockService());
	session new_session(io_context, config, service_handlers, max_len);

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
	new_session.change_data("GET /echo HTTP/1.1\r\n\r\n");
	std::string normal_response_test = new_session.construct_response(max_len);
	pos = normal_response_test.find(http_ok);
	EXPECT_EQ(pos, 0);

	// test bad request (no service handler)
	service_handlers.clear();
	new_session.change_data("GET /echo HTTP/1.1\r\n\r\n");
	std::string bad_req_test = new_session.construct_response(max_len);
	pos = bad_req_test.find(http_ok);
	EXPECT_EQ(pos, std::string::npos);
}