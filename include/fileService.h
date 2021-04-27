#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <string>

#include "service.h"

namespace http = boost::beast::http;
namespace fs = boost::filesystem;

class FileService : public Service {
   public:
	FileService(std::string prefix, std::string linux_dir);

	std::string make_response(http::request<http::string_body> req);
	bool can_handle(http::request<http::string_body> req);

	std::string get_mime(std::string target);
   
   private:
	std::string url_prefix;
	fs::path linux_dir;
};