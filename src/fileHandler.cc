#include "fileHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;
namespace fs = boost::filesystem;

FileHandler::FileHandler(const std::string& prefix, const NginxConfig& config)
    : url_prefix(prefix) {
	// Ignore tailing slashes while registering urls
	if (url_prefix.size() > 0 && url_prefix[url_prefix.size() - 1] == '/') {
		url_prefix.erase(url_prefix.size() - 1);
	}

	try {
		linux_dir = config.statements_.at(0)->tokens_.at(1);
		invalid_config = false;
	} catch (std::exception& e) {
		FATAL << "exception occurred : " << e.what();
		invalid_config = true;
	}
}

http::response<http::string_body> FileHandler::handle_request(const http::request<http::string_body>& request) {
	std::string target(request.target());
	fs::path linux_path;

	if (invalid_config) {
		return RequestHandler::internal_server_error();
	}

	// Ignore trailing slashes
	if (target.size() > 0 && target[target.size() - 1] == '/') {
		target.erase(target.size() - 1);
	}
	INFO << "target is: " << target;

	// Check that linux_dir exists
	if (!fs::exists(linux_dir)) {
		INFO << "file handler serving on non-existant linux path: " << linux_dir;
		return not_found_error();
	}

	if (!fs::is_directory(linux_dir)) {
		// If linux_dir is a file actually, ignore the rest of the url
		INFO << "file handler registered with a file directly: " << linux_dir;
		linux_path = linux_dir;
	} else {
		// Construct the file path from the url
		INFO << "file handler registered with a directory: " << linux_dir;

		fs::path filepath(target.substr(url_prefix.size()));
		linux_path = linux_dir / filepath;

		// Check if file exists
		if (!fs::exists(linux_path) || fs::is_directory(linux_path)) {
			ERROR << "file handler could not find path: " << linux_path;
			return not_found_error();
		}
	}

	http::response<http::string_body> res;
	std::ifstream file(linux_path.c_str());
	std::string filebody;

	// Allocate a large enough string
	file.seekg(0, std::ios::end);
	filebody.reserve(file.tellg());
	file.seekg(0, std::ios::beg);

	// Load the file in a string
	filebody.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	res.version(11);  // HTTP/1.1
	res.result(http::status::ok);
	res.set(http::field::content_type, get_mime(target));
	res.set(http::field::server, "koko.cs130.org");
	res.body() = filebody;
	res.prepare_payload();
	return res;
}

std::string getExtension(std::string filename) {
	auto pos = filename.rfind(".");
	if (pos == std::string::npos) {
		return "";
	}
	return filename.substr(pos);
}

// Adapted from
// https://www.boost.org/doc/libs/develop/libs/beast/example/advanced/server/advanced_server.cpp
std::string FileHandler::get_mime(std::string target) {
	std::string extension = getExtension(target);
	if (extension == ".htm") return "text/html";
	if (extension == ".html") return "text/html";
	if (extension == ".php") return "text/html";
	if (extension == ".css") return "text/css";
	if (extension == ".txt") return "text/plain";
	if (extension == ".js") return "application/javascript";
	if (extension == ".json") return "application/json";
	if (extension == ".xml") return "application/xml";
	if (extension == ".swf") return "application/x-shockwave-flash";
	if (extension == ".flv") return "video/x-flv";
	if (extension == ".png") return "image/png";
	if (extension == ".jpe") return "image/jpeg";
	if (extension == ".jpeg") return "image/jpeg";
	if (extension == ".jpg") return "image/jpeg";
	if (extension == ".gif") return "image/gif";
	if (extension == ".bmp") return "image/bmp";
	if (extension == ".ico") return "image/vnd.microsoft.icon";
	if (extension == ".tiff") return "image/tiff";
	if (extension == ".tif") return "image/tiff";
	if (extension == ".svg") return "image/svg+xml";
	if (extension == ".svgz") return "image/svg+xml";
	if (extension == ".zip") return "application/zip";
	return "text/plain";
}

fs::path FileHandler::get_linux_dir() {
	return linux_dir;
}