// An nginx config file parser.
//
// See:
//   http://wiki.nginx.org/Configuration
//   http://blog.martinfjordvald.com/2010/07/nginx-primer/
//
// How Nginx does it:
//   http://lxr.nginx.org/source/src/core/ngx_conf_file.c

#include "parser.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "logger.h"

const std::string NginxConfigParser::escape_chars = "abfnrtv'\"?\\";

NginxConfigParser::NginxConfigParser() {}

const char *NginxConfigParser::TokenTypeAsString(TokenType type) {
	switch (type) {
	case TOKEN_TYPE_START:
		return "TOKEN_TYPE_START";
	case TOKEN_TYPE_NORMAL:
		return "TOKEN_TYPE_NORMAL";
	case TOKEN_TYPE_START_BLOCK:
		return "TOKEN_TYPE_START_BLOCK";
	case TOKEN_TYPE_END_BLOCK:
		return "TOKEN_TYPE_END_BLOCK";
	case TOKEN_TYPE_COMMENT:
		return "TOKEN_TYPE_COMMENT";
	case TOKEN_TYPE_STATEMENT_END:
		return "TOKEN_TYPE_STATEMENT_END";
	case TOKEN_TYPE_EOF:
		return "TOKEN_TYPE_EOF";
	case TOKEN_TYPE_ERROR:
		return "TOKEN_TYPE_ERROR";
	default:
		return "Unknown token type";
	}
}

NginxConfigParser::TokenType NginxConfigParser::ParseToken(std::istream *input,
                                                           std::string *value) {
	TokenParserState state = TOKEN_STATE_INITIAL_WHITESPACE;
	if (input == nullptr || value == nullptr) {
		return TOKEN_TYPE_ERROR;
	}

	while (input->good()) {
		const char c = input->get();
		if (!input->good()) {
			break;
		}
		switch (state) {
		case TOKEN_STATE_INITIAL_WHITESPACE:
			switch (c) {
			case '{':
				*value = c;
				return TOKEN_TYPE_START_BLOCK;
			case '}':
				*value = c;
				return TOKEN_TYPE_END_BLOCK;
			case '#':
				*value = c;
				state = TOKEN_STATE_TOKEN_TYPE_COMMENT;
				continue;
			case '"':
				*value = c;
				state = TOKEN_STATE_DOUBLE_QUOTE;
				continue;
			case '\'':
				*value = c;
				state = TOKEN_STATE_SINGLE_QUOTE;
				continue;
			case ';':
				*value = c;
				return TOKEN_TYPE_STATEMENT_END;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				continue;
			default:
				*value += c;
				state = TOKEN_STATE_TOKEN_TYPE_NORMAL;
				continue;
			}
		case TOKEN_STATE_SINGLE_QUOTE:
			*value += c;
			if (c == '\\') {
				std::string err = consumeEscapeChar(input, value, '\'');
				if (err != "") {
					ERROR << "unable to parse escape character in config, err: " << err;
					return TOKEN_TYPE_ERROR;
				}
				continue;
			}
			if (c == '\'') {
				char peeked_char = input->peek();
				// Allowing ; after quotes
				if (!(std::isspace(peeked_char) || peeked_char == ';')) {
					return TOKEN_TYPE_ERROR;
				}
				removeQuotes(value, '\'');
				return TOKEN_TYPE_NORMAL;
			}
			continue;

		case TOKEN_STATE_DOUBLE_QUOTE:
			*value += c;
			if (c == '\\') {
				std::string err = consumeEscapeChar(input, value, '"');
				if (err != "") {
					ERROR << "unable to parse escape character in config, err: " << err;
					return TOKEN_TYPE_ERROR;
				}
				continue;
			}
			if (c == '"') {
				char peeked_char = input->peek();
				// Allowing ; after quotes
				if (!(std::isspace(peeked_char) || peeked_char == ';')) {
					return TOKEN_TYPE_ERROR;
				}
				removeQuotes(value, '"');
				return TOKEN_TYPE_NORMAL;
			}
			continue;
		case TOKEN_STATE_TOKEN_TYPE_COMMENT:
			if (c == '\n' || c == '\r') {
				return TOKEN_TYPE_COMMENT;
			}
			*value += c;
			continue;
		case TOKEN_STATE_TOKEN_TYPE_NORMAL:
			if (c == ' ' || c == '\t' || c == '\n' ||
			    c == ';' || c == '{' || c == '}') {
				input->unget();
				return TOKEN_TYPE_NORMAL;
			}
			*value += c;
			continue;
		}
	}

	// If we get here, we reached the end of the file.
	if (state == TOKEN_STATE_SINGLE_QUOTE ||
	    state == TOKEN_STATE_DOUBLE_QUOTE) {
		return TOKEN_TYPE_ERROR;
	}

	return TOKEN_TYPE_EOF;
}

std::string NginxConfigParser::consumeEscapeChar(std::istream *input, std::string *value, char quote) {
	if (!(quote == '\'' || quote == '"')) {
		return "string cannot be quoted with given quote in the config";
	}

	// first-level escape
	char peeked_char = input->peek();
	if (NginxConfigParser::escape_chars.find(peeked_char) == std::string::npos) {
		return "non-escape char following \\ in config";
	}
	// consume the \ char and remove it from the token
	input->get();
	value->at(value->size() - 1) = peeked_char;

	// check double-escaping for quotes only
	// eg: \\' to include a quote in a string surrounded by ''
	if (peeked_char == '\\') {
		if (input->peek() == quote) {
			// consume the extra \ too and place the ' in the token
			input->get();
			value->at(value->size() - 1) = quote;
		}
	}
	return "";
}

void NginxConfigParser::removeQuotes(std::string *value, char quote) {
	if (value->size() > 2 &&
	    value->at(0) == quote && value->at(value->size() - 1) == quote) {
		// Remove the first and the last quote
		value->erase(value->size() - 1, 1);
		value->erase(0, 1);
	}
}

bool NginxConfigParser::Parse(std::istream *config_file, NginxConfig *config) {
	if (config == nullptr) {
		return false;
	}

	std::stack<NginxConfig *> config_stack;
	config_stack.push(config);
	TokenType last_token_type = TOKEN_TYPE_START;
	TokenType token_type;
	int braces_depth = 0;
	while (true) {
		std::string token;
		token_type = ParseToken(config_file, &token);
		if (token_type == TOKEN_TYPE_ERROR) {
			break;
		}

		if (token_type == TOKEN_TYPE_COMMENT) {
			// Skip comments.
			continue;
		}

		if (token_type == TOKEN_TYPE_START) {
			// Error.
			break;
		} else if (token_type == TOKEN_TYPE_NORMAL) {
			if (last_token_type == TOKEN_TYPE_START ||
			    last_token_type == TOKEN_TYPE_STATEMENT_END ||
			    last_token_type == TOKEN_TYPE_START_BLOCK ||
			    last_token_type == TOKEN_TYPE_END_BLOCK ||
			    last_token_type == TOKEN_TYPE_NORMAL) {
				if (last_token_type != TOKEN_TYPE_NORMAL) {
					config_stack.top()->statements_.emplace_back(
					    new NginxConfigStatement);
				}
				config_stack.top()->statements_.back().get()->tokens_.push_back(
				    token);
			} else {
				// Error.
				break;
			}
		} else if (token_type == TOKEN_TYPE_STATEMENT_END) {
			if (last_token_type != TOKEN_TYPE_NORMAL) {
				// Error.
				break;
			}
		} else if (token_type == TOKEN_TYPE_START_BLOCK) {
			if (last_token_type != TOKEN_TYPE_NORMAL) {
				// Error.
				break;
			}
			NginxConfig *const new_config = new NginxConfig;
			config_stack.top()->statements_.back().get()->child_block_.reset(
			    new_config);
			config_stack.push(new_config);
			++braces_depth;
		} else if (token_type == TOKEN_TYPE_END_BLOCK) {
			if (last_token_type != TOKEN_TYPE_STATEMENT_END &&
			    last_token_type != TOKEN_TYPE_END_BLOCK &&
			    last_token_type != TOKEN_TYPE_START_BLOCK) {
				// Error.
				break;
			}

			--braces_depth;
			if (braces_depth < 0) {
				//Error
				break;
			}
			config_stack.pop();

		} else if (token_type == TOKEN_TYPE_EOF) {
			if ((last_token_type != TOKEN_TYPE_STATEMENT_END &&
			     last_token_type != TOKEN_TYPE_END_BLOCK) ||
			    (braces_depth != 0)) {
				// Error.
				break;
			}
			return true;
		} else {
			// Error. Unknown token.
			break;
		}
		last_token_type = token_type;
	}

	printf("Bad transition from %s to %s\n",
	       TokenTypeAsString(last_token_type),
	       TokenTypeAsString(token_type));
	return false;
}

bool NginxConfigParser::Parse(const char *file_name, NginxConfig *config) {
	std::ifstream config_file;
	config_file.open(file_name);
	if (!config_file.is_open()) {
		std::cerr << "Failed to open config file: " << file_name << std::endl;
		return false;
	}

	const bool return_value =
	    Parse(dynamic_cast<std::istream *>(&config_file), config);
	config_file.close();
	return return_value;
}

int NginxConfigParser::parse_args(int argc, const char *argv[], NginxConfig *config) {
	INFO << "starting to parse program arguments";

	if (argc != 2) {
		FATAL << "missing config file, usage: webserver <config_file_path>";
		exit(1);
	}

	if (config == nullptr) {
		FATAL << "need a config to store the parsed arguments, given nullptr";
		exit(2);
	}

	// Parse the config
	NginxConfigParser config_parser;
	if (!config_parser.Parse(argv[1], config)) {
		FATAL << "failed to parse config";
	}
	INFO << "parsed the raw config";

	config->extract_targets();

	return 0;
}