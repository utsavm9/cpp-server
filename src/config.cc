// An nginx config file parser.
//
// See:
//   http://wiki.nginx.org/Configuration
//   http://blog.martinfjordvald.com/2010/07/nginx-primer/
//
// How Nginx does it:
//   http://lxr.nginx.org/source/src/core/ngx_conf_file.c

#include "config.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "logger.h"
#include "parser.h"

NginxConfig::NginxConfig() : port(80), urlToLinux{}, urlToServiceName{{"/", "echo"}} {
}

void NginxConfig::free_memory() {
	statements_.clear();
}

void NginxConfig::extract_port() {
	for (const auto &statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			// In top-level server block
			for (const auto &substatement : statement->child_block_->statements_) {
				if (substatement->tokens_.size() > 0 && substatement->tokens_[0] == "listen") {
					// In port statement
					if (substatement->tokens_.size() == 2) {
						try {
							port = std::stoi(substatement->tokens_[1]);
							BOOST_LOG_SEV(slg::get(), info) << "extracted port number from config, setting port " << port;
						} catch (std::out_of_range) {
							BOOST_LOG_SEV(slg::get(), error) << "Port Number Too Large";
						} catch (std::invalid_argument) {
							BOOST_LOG_SEV(slg::get(), info) << "Malformed Port Number ";
						}
					} else {
						BOOST_LOG_SEV(slg::get(), error) << "expecting exactly one port number field here";
					}
				}
			}
		}
	}
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
							BOOST_LOG_SEV(slg::get(), error) << "Missing Filesystem mapping: " << path_reg->tokens_[0];
							continue;
						}

						else if (path_reg->tokens_.size() > 2) {
							BOOST_LOG_SEV(slg::get(), error) << "Too many tokens in Static Path mapping: " << path_reg->tokens_[0];
							continue;
						}

						urlToServiceName.push_back({path_reg->tokens_[0], "static"});
						urlToLinux.insert({path_reg->tokens_[0], path_reg->tokens_[1]});
						BOOST_LOG_SEV(slg::get(), info) << "Registered Static Path Mapping: " << path_reg->tokens_[0] << " -> " << path_reg->tokens_[1];
					}
				}
				if (substatement->tokens_.size() > 0 && substatement->tokens_[0] == "echo") {
					for (const auto &path_reg : substatement->child_block_->statements_) {
						if (path_reg->tokens_.size() > 1) {
							BOOST_LOG_SEV(slg::get(), error) << "Too many tokens in Echo path registration: " << path_reg->tokens_[0];
							continue;
						}
						urlToServiceName.push_back({path_reg->tokens_[0], "echo"});
						BOOST_LOG_SEV(slg::get(), info) << "Registered Echo Path: " << path_reg->tokens_[0];
					}
				}
			}
		}
	}
}