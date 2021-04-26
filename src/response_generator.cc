#include "response_generator.h"

#include <iostream>

#include "logger.h"

std::string Response_Generator::generate_response(char* read_buf, size_t bytes_transferred, size_t max_buf_size) {
	if (bytes_transferred > (max_buf_size - 1)) {  //this method shouldn't be called with these parameters
		BOOST_LOG_SEV(slg::get(), error) << "Bad Parameter for response_generator::generate_response(), Please check your buffer size for read_buf";
		return "";
	}

	// Ensure that the data is null terminated
	if (bytes_transferred < 0) {
		read_buf[0] = '\0';
	} else if (bytes_transferred >= max_buf_size - 1) {
		read_buf[max_buf_size - 1] = '\0';
	} else {
		read_buf[bytes_transferred] = '\0';
	}

	// Construct the response
	std::string http_response =
	    "HTTP/1.1 200 OK\r\n"
	    "Content-Type: text/plain\r\n"
	    "Content-Length: " +
	    std::to_string(bytes_transferred) +
	    "\r\n"
	    "\r\n" +
	    std::string(read_buf);

	BOOST_LOG_SEV(slg::get(), debug) << "HTTP RESPONSE: " << http_response;

	return http_response;
}