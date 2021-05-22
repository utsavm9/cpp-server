#include "proxyRequestHandler.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "logger.h"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

ProxyRequestHandler::ProxyRequestHandler(const std::string &p, const NginxConfig &config)
    : url_prefix(p) {
	name = "ProxyRequest";

	if (url_prefix.size() > 0 && url_prefix[url_prefix.size() - 1] == '/') {
		url_prefix.erase(url_prefix.size() - 1);
	}
	try {
		for (size_t i = 0; i < config.statements_.size(); i++) {
			std::shared_ptr<NginxConfigStatement> st = config.statements_.at(i);
			if (!strcmp(st->tokens_.at(0).c_str(), "dest")) {
				proxy_dest = st->tokens_.at(1);
			}
			if (!strcmp(st->tokens_.at(0).c_str(), "port")) {
				m_port = st->tokens_.at(1);
			}
		}
		if (proxy_dest.empty() || m_port.empty()) {
			throw std::runtime_error("Missing required config field");
		}
		TRACE << "ProxyRequestHandler for " << url_prefix << " -> Proxy destination: " << proxy_dest;
		TRACE << "ProxyRequestHandler for " << url_prefix << " -> Port: " << m_port;
		invalid_config = false;
	} catch (std::exception &e) {
		FATAL << "exception occurred : " << e.what();
		invalid_config = true;
	}
}

http::response<http::string_body> ProxyRequestHandler::req_synchronous(const http::request<http::string_body> &request) {
	net::io_context ioc;
	tcp::resolver resolver(ioc);
	beast::tcp_stream stream(ioc);
	tcp::resolver::query query(proxy_dest, m_port);
	auto const results = resolver.resolve(query);
	TRACE << "ProxyRequestHandler attempting to connect to host "
	      << proxy_dest
	      << " on port "
	      << m_port;
	stream.connect(results);
	TRACE << "ProxyRequestHandler successfully connected to host "
	      << proxy_dest
	      << " on "
	      << stream.socket().remote_endpoint().address().to_string()
	      << ":"
	      << stream.socket().remote_endpoint().port();
	http::write(stream, request);
	beast::flat_buffer buffer;
	http::response<http::string_body> res;
	beast::error_code ec;
	http::read(stream, buffer, res, ec);
	stream.socket().shutdown(tcp::socket::shutdown_both, ec);
	if (ec && ec != beast::errc::not_connected) {
		throw beast::system_error{ec};
	}
	res.prepare_payload();
	return res;
}

void ProxyRequestHandler::replace_relative_html_links(std::string &body) {
	if (url_prefix == "/" || url_prefix.length() == 0) {
		return;
	}
	std::string encoded_prefix(url_prefix);
	boost::replace_all(encoded_prefix, "/", "\\2f");

	boost::replace_all(body, "href=\"/", "href=\"" + url_prefix + "/");
	boost::replace_all(body, "src=\"/", "src=\"" + url_prefix + "/");
	boost::replace_all(body, "url(\"/", "url(\"" + url_prefix + "/");
	boost::replace_all(body, "url(\\2f", "url(" + encoded_prefix + "\\2f");

	// Revert the original absolute paths that used "//":
	// Eg. href="//washington.edu" -> href="/uw//washington.edu" -> href="//washington.edu"
	boost::replace_all(body, "href=\"" + url_prefix + "//", "href=\"//");
	boost::replace_all(body, "src=\"" + url_prefix + "//", "src=\"//");
	boost::replace_all(body, "url(\"" + url_prefix + "//", "url(\"//");
	boost::replace_all(body, "url(" + encoded_prefix + "\\2f\\2f", "url(\\2f\\2f");
}

http::response<http::string_body> ProxyRequestHandler::handle_request(const http::request<http::string_body> &request) {
	if (invalid_config) {
		return RequestHandler::internal_server_error();
	}
	try {
		http::request<http::string_body> proxy_req(request);
		proxy_req.set(http::field::host, proxy_dest);
		std::string target = request.target().substr(url_prefix.length()).to_string();
		if (target.length() == 0) {
			target = "/";
		}
		if (target[0] != '/') {
			target = "/" + target;
		}
		proxy_req.target(target);
		// Only accept unencoded values so we can modify the body
		proxy_req.set(http::field::accept_encoding, "identity");
		proxy_req.prepare_payload();
		http::response<http::string_body> res = req_synchronous(proxy_req);

		if (http::to_status_class(res.result()) != http::status_class::redirection ||
		    res.find(http::field::location) == res.end()) {
			if (res.find(http::field::content_type) != res.end() && res.at(http::field::content_type).to_string().find("text/html") != std::string::npos) {
				replace_relative_html_links(res.body());
			}
			return res;
		}

		// Handle redirect
		std::string redirect_location = res.at(http::field::location).to_string();
		if (redirect_location.length() > 0 && redirect_location[0] == '/') {
			// Relative url redirects should be made relative to the proxy
			redirect_location = url_prefix + redirect_location;
		}
		res.set(http::field::location, redirect_location);
		return res;
	} catch (std::exception &e) {
		ERROR << "Exception occurred in ProxyRequestHandler for " << url_prefix << ": " << e.what();
		return RequestHandler::internal_server_error();
	}
}

std::string ProxyRequestHandler::get_url_prefix() {
	return url_prefix;
}