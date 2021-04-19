#include "response_generator.h"

#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"

//Class to create HTTP respoonse (without content length)
class HTTPResponses {
   public:
	HTTPResponses() {
		std::string response_200 =
		    "HTTP/1.1 200 OK\r\n"
		    "Content-Type: text/plain\r\n"
		    "Content-Length: ";

		response[200] = response_200;
	}

	std::unordered_map<int, std::string> response;
};

class ResponseGeneratorTest : public ::testing::Test {
   protected:
	std::string get_response(char* read_buf, size_t bytes_transferred, size_t max_buf_size) {
		return g.generate_response(read_buf, bytes_transferred, max_buf_size);
	}

	Response_Generator g;
	HTTPResponses http;
};

TEST_F(ResponseGeneratorTest, echoTest) {
	char input1[30] = "hello world";
	size_t bytes1 = std::strlen(input1);

	std::string expected_response1 = http.response[200] + std::to_string(bytes1) +
	                                 "\r\n"
	                                 "\r\n" +
	                                 std::string(input1);

	std::string temp1 = get_response(input1, bytes1, 30);
	EXPECT_TRUE(expected_response1 == temp1);

	char input2[30] = "CS130 Winter 2020\r\n";
	size_t bytes2 = std::strlen(input2);

	std::string expected_response2 = http.response[200] + std::to_string(bytes2) +
	                                 "\r\n"
	                                 "\r\n" +
	                                 std::string(input2);

	std::string temp2 = get_response(input2, bytes2, 30);
	EXPECT_TRUE(expected_response2 == temp2);
}