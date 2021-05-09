// An nginx config file parser.
//
// See:
//   http://wiki.nginx.org/Configuration
//   http://blog.martinfjordvald.com/2010/07/nginx-primer/
//
// How Nginx does it:
//   http://lxr.nginx.org/source/src/core/ngx_conf_file.c

#include "config.h"

#include <boost/optional.hpp>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "logger.h"
#include "parser.h"

using boost::optional;

NginxConfig::NginxConfig() : urlToServiceName{{"/", "echo"}}, urlToLinux{} {
}

void NginxConfig::free_memory() {
	statements_.clear();
}

int NginxConfig::get_port() {
	// Function to get extract port from statement if port is present.
	auto get_port_from_statement = [](std::shared_ptr<NginxConfigStatement> statement) {
		if (statement->tokens_.size() > 0 && statement->tokens_[0] == "port") {
			// Inside a port statement
			if (statement->tokens_.size() != 2) {
				ERROR << "found a malformed port level-0 line in config with this many token: " << statement->tokens_.size();
				return optional<int>{};
			}

			// Try getting the port number
			try {
				return optional<int>{std::stoi(statement->tokens_[1])};
			} catch (std::out_of_range const &) {
				ERROR << "port number too large";
			} catch (std::invalid_argument const &) {
				INFO << "malformed port Number ";
			}
		}
		return optional<int>{};
	};

	// Do a level-0 scan for port
	for (const auto &statement : statements_) {
		optional<int> port = get_port_from_statement(statement);
		if (port.is_initialized()) {
			INFO << "extracted port number from config, setting port " << port.value();
			return port.value();
		}
	}

	// Do a level-1 scan for port number inside the server block
	for (const auto &statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			// In top-level server block
			for (const auto &substatement : statement->child_block_->statements_) {
				optional<int> port = get_port_from_statement(substatement);
				if (port.is_initialized()) {
					INFO << "extracted port number from config server block, setting port " << port.value();
					return port.value();
				}
			}
		}
	}

	// default port number
	INFO << "failed to find a port number from config, using 80 by default";
	return 80;
}

void NginxConfig::extract_targets() {
	urlToLinux.clear();
	urlToServiceName.clear();

	for (const auto &statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			for (const auto &substatement : statement->child_block_->statements_) {
				if (substatement->tokens_.size() > 0 && substatement->tokens_[0] == "static") {
					for (const auto &path_reg : substatement->child_block_->statements_) {
						if (path_reg->tokens_.size() < 2) {
							ERROR << "missing filesystem mapping: " << path_reg->tokens_[0];
							continue;
						}

						else if (path_reg->tokens_.size() > 2) {
							ERROR << "too many tokens in static path mapping: " << path_reg->tokens_[0];
							continue;
						}

						urlToServiceName.push_back({path_reg->tokens_[0], "static"});
						urlToLinux.insert({path_reg->tokens_[0], path_reg->tokens_[1]});
						INFO << "registered static path mapping: " << path_reg->tokens_[0] << " -> " << path_reg->tokens_[1];
					}
				}
				if (substatement->tokens_.size() > 0 && substatement->tokens_[0] == "echo") {
					for (const auto &path_reg : substatement->child_block_->statements_) {
						if (path_reg->tokens_.size() > 1) {
							ERROR << "too many tokens in echo path registration: " << path_reg->tokens_[0];
							continue;
						}
						urlToServiceName.push_back({path_reg->tokens_[0], "echo"});
						INFO << "registered echo path: " << path_reg->tokens_[0];
					}
				}
			}
		}
	}
}