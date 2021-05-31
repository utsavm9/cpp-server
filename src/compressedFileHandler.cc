#include "compressedFileHandler.h"

CompressedFileHandler::CompressedFileHandler(const std::string& prefix, const NginxConfig& config)
    : FileHandler(prefix, config) {
	name = "File";

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

std::string compress(const std::string& data) {
	namespace bio = boost::iostreams;

	std::stringstream compressed;
	std::stringstream origin(data);

	bio::filtering_streambuf<bio::input> out;
	out.push(bio::gzip_compressor(bio::gzip_params(bio::gzip::best_compression)));
	out.push(origin);
	bio::copy(out, compressed);

	return compressed.str();
}

http::response<http::string_body> CompressedFileHandler::handle_request(const http::request<http::string_body>& request) {
	http::response<http::string_body> res = FileHandler::handle_request(request);
	std::string body = res.body();
	std::string compressedBody = compress(body);
	res.body() = compressedBody;
	res.set(http::field::content_encoding, "gzip");
	res.set(http::field::content_length, compressedBody.size());
	return res;
}