#pragma once

#include <string>

class Response_Generator {
   public:
	std::string generate_response(char* read_buf, size_t bytes_transferred, size_t max_buf_size);
};