#include "fileService.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <string>

#include "logger.h"

namespace http = boost::beast::http;
namespace fs = boost::filesystem;

FileService::FileService(std::string prefix, std::string l)
    : url_prefix(prefix), linux_dir(l) {
}

std::string FileService::make_response(http::request<http::string_body> req) {
	std::string target(req.target());
	// Identify if index.html is needed
	if (target.size() > 0 && target[target.size() - 1] == '/') {
		target.append("index.html");
	}

	fs::path filepath(target.substr(url_prefix.size()));
	fs::path linux_path(linux_dir / filepath);

	// Check if file exists
	if (!fs::exists(linux_path)) {
		ERROR << "could not find path " << linux_path;
		return not_found_error();
	}

	http::response<http::string_body> res;
	std::ifstream file(linux_path.c_str());
	std::string filebody;

	// Allocate a large enough string
	file.seekg(0, std::ios::end);
	filebody.reserve(file.tellg());
	file.seekg(0, std::ios::beg);

	// Load the file in a string
	// Currently, extra large files not supported
	filebody.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	res.version(11);  // HTTP/1.1
	res.result(http::status::ok);
	res.set(http::field::content_type, get_mime(target));
	res.set(http::field::server, "koko.cs130.org");
	res.body() = filebody;
	res.prepare_payload();

	return to_string(res);
}

bool FileService::can_handle(http::request<http::string_body> req) {
	std::string target(req.target());

	// Check if target is valid
	if (target.empty()) {
		INFO << "did not find target in request, cannot serve a static file";
		return false;
	}

	if (target[0] != '/') {
		INFO << "target not starting with '/', cannot serve a static file";
		return false;
	}

	if (target.find("..") != std::string::npos) {
		INFO << "target contains relative paths, refusing to use relative paths to serve files";
		return false;
	}

	// Check if this service is supposed to serve this target
	size_t prefix_len = url_prefix.size();
	return prefix_len <= target.size() && target.substr(0, prefix_len) == url_prefix;
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
std::string FileService::get_mime(std::string target) {
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