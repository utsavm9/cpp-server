#include <boost/algorithm/string/replace.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "config.h"
#include "handler.h"

namespace http = boost::beast::http;

class ProxyRequestHandler : public RequestHandler {
   public:
	ProxyRequestHandler(const std::string &url_prefix, const NginxConfig &config);

	std::string get_url_prefix();

   protected:
	virtual http::response<http::string_body> handle_request(const http::request<http::string_body> &request) override;

   private:
	virtual http::response<http::string_body> req_synchronous(const http::request<http::string_body> &request);
	void replace_relative_html_links(std::string &body);
	// Serve for these url suffixes. eg. "/" to serve all valid targets
	std::string url_prefix;
	std::string proxy_dest;
	std::string m_port;
	std::size_t max_redirects;
	bool invalid_config;
};