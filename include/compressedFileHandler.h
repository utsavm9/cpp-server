#pragma once

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <sstream>

#include "fileHandler.h"

namespace http = boost::beast::http;
namespace fs = boost::filesystem;

class CompressedFileHandler : public FileHandler {
   public:
	CompressedFileHandler(const std::string& url_prefix, const NginxConfig& config);

   protected:
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request) override;
};