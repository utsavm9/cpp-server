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
	max_redirects = 2;
	try {
		for (size_t i = 0; i < config.statements_.size(); i++) {
			std::shared_ptr<NginxConfigStatement> st = config.statements_.at(i);
			if (!strcmp(st->tokens_.at(0).c_str(), "dest")) {
				proxy_dest = st->tokens_.at(1);
			}
			if (!strcmp(st->tokens_.at(0).c_str(), "port")) {
				m_port = st->tokens_.at(1);
			}
			if (!strcmp(st->tokens_.at(0).c_str(), "max_redirects")) {
				max_redirects = std::stoi(st->tokens_.at(1));
			}
		}
		if (proxy_dest.empty() || m_port.empty()) {
			throw std::runtime_error("Missing required config field");
		}
		INFO << "ProxyRequestHandler for " << url_prefix << " -> Proxy destination: " << proxy_dest;
		INFO << "ProxyRequestHandler for " << url_prefix << " -> Port: " << m_port;
		INFO << "ProxyRequestHandler for " << url_prefix << " -> Max Redirects: " << max_redirects;
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
	stream.connect(results);
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

		std::size_t redirect_count = 0;
		// Automatically resolve http redirects
		while (res.result_int() == 301 || res.result_int() == 302) {
			if (redirect_count >= max_redirects) {
				throw std::runtime_error("Too many redirects");
			}
			std::string host;
			std::string target = "/";
			std::string location = res.at(http::field::location).to_string();

			// Server currently cannot handle https as per piazza @225
			if (location.find("https://") != std::string::npos) {
				throw std::runtime_error("HTTPS redirects unsupported");
			}

			// Following location formats supported:
			// 1. http://host.com/path -> new host, new target
			// 2. host.com/path -> new host, new target
			// 3. host.com -> new host, old target
			// 4. /path -> old host, new target
			std::size_t scheme_end = location.find("//");
			if (scheme_end != std::string::npos) {
				location = location.substr(scheme_end + 2);
			}
			std::size_t path_start = location.find("/");
			if (path_start != std::string::npos) {
				target = location.substr(path_start);
				host = location.substr(0, path_start);
			} else {
				host = location;
			}
			if (host.length() > 0) {
				proxy_req.set(http::field::host, host);
			}
			proxy_req.target(target);
			res = req_synchronous(host, m_port, proxy_req);
			redirect_count++;
		}
		return res;
	} catch (std::exception &e) {
		ERROR << "Exception occurred in ProxyRequestHanlder for " << url_prefix << ": " << e.what();
		return RequestHandler::internal_server_error();
	}
}

std::string ProxyRequestHandler::get_url_prefix() {
	return url_prefix;
}