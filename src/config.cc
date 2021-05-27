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

std::unordered_map<std::string, short> NginxConfig::defaults = {{"port", 80}, {"threads", 4}, {"httpsPort", 443}};

NginxConfig::NginxConfig() {
}

int NginxConfig::get_field(std::string field) {
	// Function to get extract field from statement if field is present.
	auto get_field_from_statement = [field](std::shared_ptr<NginxConfigStatement> statement) {
		if (statement->tokens_.size() > 0 && statement->tokens_[0] == field) {
			// Inside a field statement
			if (statement->tokens_.size() != 2) {
				ERROR << "found a malformed field level-0 line in config with this many token: " << statement->tokens_.size();
				return optional<int>{};
			}

			// Try getting the field number
			try {
				return optional<int>{std::stoi(statement->tokens_[1])};
			} catch (std::out_of_range const &) {
				ERROR << "field number too large";
			} catch (std::invalid_argument const &) {
				TRACE << "malformed field Number ";
			}
		}
		return optional<int>{};
	};

	// Do a level-0 scan for field
	for (const auto &statement : statements_) {
		optional<int> field_num = get_field_from_statement(statement);
		if (field_num.is_initialized()) {
			TRACE << "extracted field number from config, setting field " << field_num.value();
			return field_num.value();
		}
	}

	// Do a level-1 scan for field number inside the server block
	for (const auto &statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			// In top-level server block
			for (const auto &substatement : statement->child_block_->statements_) {
				optional<int> field_num = get_field_from_statement(substatement);
				if (field_num.is_initialized()) {
					TRACE << "extracted field number from config server block, setting field " << field_num.value();
					return field_num.value();
				}
			}
		}
	}

	try {
		return defaults.at(field);
	} catch (std::out_of_range const &) {
		ERROR << "default field number not found";
		return 0;
	}
}