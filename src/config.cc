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

NginxConfig::NginxConfig() : port(80),
                             targets{{"echo", {"/"}}, {"static", {"/"}}} {
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
	std::unordered_map<std::string, std::vector<std::string>> path_map;

	for (const auto &statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			for (const auto &substatement : statement->child_block_->statements_) {
				if (substatement->tokens_.size() > 0 && substatement->tokens_[0] == "register_paths") {
					//Check for approprate directives in register_paths block

					for (const auto &path_reg : substatement->child_block_->statements_) {
						if (path_reg->tokens_.size() > 0 && path_reg->tokens_.size() < 3) {
							//register static path

							if (path_reg->tokens_[0] == "static") {
								if (path_map.find("static") == path_map.end()) {
									std::vector<std::string> static_paths;
									static_paths.push_back(path_reg->tokens_[1]);
									path_map.insert(std::make_pair("static", static_paths));
									BOOST_LOG_SEV(slg::get(), info) << "Registered static path " << path_reg->tokens_[1];
								} else {
									path_map["static"].push_back(path_reg->tokens_[1]);
									BOOST_LOG_SEV(slg::get(), info) << "Registered static path " << path_reg->tokens_[1];
								}
							}
							// register echo path

							else if (path_reg->tokens_[0] == "echo") {
								if (path_map.find("echo") == path_map.end()) {
									std::vector<std::string> static_paths;
									static_paths.push_back(path_reg->tokens_[1]);
									path_map.insert(std::make_pair("echo", static_paths));
									BOOST_LOG_SEV(slg::get(), info) << "Registered echo path " << path_reg->tokens_[1];
								} else {
									path_map["echo"].push_back(path_reg->tokens_[1]);
									BOOST_LOG_SEV(slg::get(), info) << "Registered echo path " << path_reg->tokens_[1];
								}
							}

							else {
								BOOST_LOG_SEV(slg::get(), error) << "Unexpected Directive while registering paths: " << path_reg->tokens_[0] << ". Should be echo or static!";
								return;
							}
						}

						else {
							BOOST_LOG_SEV(slg::get(), error) << "Malformed Directive: " << path_reg->tokens_[0] << ". Specify one path per directive!";
							return;
						}
					}
				}
			}
		}
	}

	targets = path_map;
}