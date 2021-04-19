#include "response_generator.h"

#include <iostream>

std::string Response_Generator::generate_response(char* read_buf, size_t bytes_transferred, size_t max_buf_size) {
	//
	if (bytes_transferred > (max_buf_size - 1)) {  //this method shouldn't be called with these parameters
		std::cerr << "Bad Parameter for response_generator::generate_response()" << std::endl;
		std::cerr << "Please check your buffer size for read_buf" << std::endl;
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

	return http_response;
}