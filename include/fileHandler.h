#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <string>

#include "config.h"
#include "handler.h"

namespace http = boost::beast::http;
namespace fs = boost::filesystem;

class FileHandler : public RequestHandler {
   public:
	FileHandler(const std::string& url_prefix, const NginxConfig& config);

	std::string get_mime(std::string target);
	fs::path get_linux_dir();

   protected:
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body>& request) override;

   private:
	std::string url_prefix;
	fs::path linux_dir;
	bool invalid_config;
};