#include "compressedFileHandler.h"

CompressedFileHandler::CompressedFileHandler(const std::string& prefix, const NginxConfig& config)
    : FileHandler(prefix, config) {
	name = "CompressedFileHandler";
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

	// Return uncompressed body if gzip is not accepted as a encoding
	bool to_compress = false;
	for (auto &h : request.base()) {
		if (h.name_string() == "Accept-Encoding") {
			if (h.value().find("gzip") != boost::beast::string_view::npos) {
				to_compress = true;
			}
			break;
		}
	}
	if (!to_compress) {
		TRACE << "compressedFileHandler: request does not accept gzip encoding, not compressing body";
		return res;
	}
	TRACE << "compressedFileHandler: request accepts gzip encoding, compressing body";

	// Compress the body
	std::string body = res.body();
	int initial_len = body.size();

	std::string compressedBody = compress(body);
	res.body() = compressedBody;
	res.set(http::field::content_encoding, "gzip");
	res.set(http::field::content_length, compressedBody.size());

	INFO << "metrics: compressedHandler reduced body size (bytes): " << (initial_len - compressedBody.size());

	return res;
}