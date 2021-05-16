#include "proxyRequestHandler.h"

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
		INFO << "ProxyRequestHandler for " << url_prefix << " -> Proxy destination: " << proxy_dest;
		INFO << "ProxyRequestHandler for " << url_prefix << " -> Port: " << m_port;
		invalid_config = false;
	} catch (std::exception &e) {
		FATAL << "exception occurred : " << e.what();
		invalid_config = true;
	}
}

http::response<http::string_body> ProxyRequestHandler::req_synchronous(std::string host, std::string port, const http::request<http::string_body> &request) {
	net::io_context ioc;
	tcp::resolver resolver(ioc);
	beast::tcp_stream stream(ioc);
	tcp::resolver::query query(host, port);
	auto const results = resolver.resolve(query);
	INFO << "ProxyRequestHandler attempting to connect to host "
	     << host
	     << " on port "
	     << port;
	stream.connect(results);
	INFO << "ProxyRequestHandler successfully connected to host "
	     << host
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
		proxy_req.prepare_payload();
		http::response<http::string_body> res = req_synchronous(proxy_dest, m_port, proxy_req);

		if (http::to_status_class(res.result()) != http::status_class::redirection ||
		    res.find(http::field::location) == res.end()) {
			return res;
		}

		// Handle redirect
		std::string redirect_location = res.at(http::field::location).to_string();
		if (redirect_location.length() > 0 && redirect_location[0] == '/') {
			// Relative url redirects should be made relative to the proxy
			redirect_location = proxy_dest + redirect_location;
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