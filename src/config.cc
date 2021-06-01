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

std::unordered_map<std::string, short> NginxConfig::default_nums = {{"port", 80}, {"threads", 4}, {"httpsPort", 443}, {"keep-alive", 0}};

NginxConfig::NginxConfig() {
}

int NginxConfig::get_num(std::string field) {
	std::string value = get_str(field);

	// Try getting the field number
	try {
		return std::stoi(value);
	} catch (std::out_of_range const &) {
		ERROR << "config: field number too large: " << value;
	} catch (std::invalid_argument const &) {
		TRACE << "config: malformed field number: " << value;
	}

	// Try returning a default number
	try {
		return default_nums.at(field);
	} catch (std::out_of_range const &) {
		ERROR << "config: default field number not found for field: " << field;
		return 0;
	}
}

boost::optional<std::string> get_str_statement(std::string field, std::shared_ptr<NginxConfigStatement> statement) {
	if (!(statement->tokens_.size() > 0 && statement->tokens_[0] == field)) {
		return optional<std::string>{};
	}

	// Inside a field statement
	if (statement->tokens_.size() != 2) {
		ERROR << "config: found a malformed field level-0 line in config with this many tokens: " << statement->tokens_.size();
		return optional<std::string>{};
	}

	return optional<std::string>{statement->tokens_[1]};
}

std::string NginxConfig::get_str(std::string field) {
	// Do a level-0 scan for field
	for (const auto &statement : statements_) {
		optional<std::string> field_str = get_str_statement(field, statement);
		if (field_str.is_initialized()) {
			TRACE << "config: extracted field from config, setting field " << field_str.value();
			return field_str.value();
		}
	}

	// Do a level-1 scan for field number inside the server block
	for (const auto &statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			// In top-level server block
			for (const auto &substatement : statement->child_block_->statements_) {
				optional<std::string> field_str = get_str_statement(field, substatement);
				if (field_str.is_initialized()) {
					TRACE << "config: extracted field from config server block, setting field " << field_str.value();
					return field_str.value();
				}
			}
		}
	}

	ERROR << "config: default field string not found for field: " << field;
	return "";
}