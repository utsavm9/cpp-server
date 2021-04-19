// An nginx config file parser.
//
// See:
//   http://wiki.nginx.org/Configuration
//   http://blog.martinfjordvald.com/2010/07/nginx-primer/
//
// How Nginx does it:
//   http://lxr.nginx.org/source/src/core/ngx_conf_file.c

#include "config_parser.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

std::string NginxConfig::ToString(int depth) {
	std::string serialized_config;
	for (const auto& statement : statements_) {
		serialized_config.append(statement->ToString(depth));
	}
	return serialized_config;
}

int NginxConfig::find_port() {
	for (const auto& statement : statements_) {
		auto tokens = statement->tokens_;
		if (tokens.size() > 0 && tokens[0] == "server") {
			// In top-level server block
			for (const auto& substatement : statement->child_block_->statements_) {
				if (substatement->tokens_.size() > 0 && substatement->tokens_[0] == "listen") {
					// In port statement
					if (substatement->tokens_.size() == 2) {
						try {
							return std::stoi(substatement->tokens_[1]);
						} catch (std::out_of_range) {
							std::cerr << "port number unbelievably large" << std::endl;
						} catch (std::invalid_argument) {
							std::cerr << "malformed port number" << std::endl;
						}
					} else {
						std::cerr << "expecting exactly one port number field here" << std::endl;
					}
				}
			}
		}
	}

	return 80;
}

std::string NginxConfigStatement::ToString(int depth) {
	std::string serialized_statement;
	for (int i = 0; i < depth; ++i) {
		serialized_statement.append("  ");
	}
	for (unsigned int i = 0; i < tokens_.size(); ++i) {
		if (i != 0) {
			serialized_statement.append(" ");
		}
		serialized_statement.append(tokens_[i]);
	}
	if (child_block_.get() != nullptr) {
		serialized_statement.append(" {\n");
		serialized_statement.append(child_block_->ToString(depth + 1));
		for (int i = 0; i < depth; ++i) {
			serialized_statement.append("  ");
		}
		serialized_statement.append("}");
	} else {
		serialized_statement.append(";");
	}
	serialized_statement.append("\n");
	return serialized_statement;
}

const char* NginxConfigParser::TokenTypeAsString(TokenType type) {
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

NginxConfigParser::TokenType NginxConfigParser::ParseToken(std::istream* input,
                                                           std::string* value) {
	TokenParserState state = TOKEN_STATE_INITIAL_WHITESPACE;
	if (input == nullptr || value == nullptr) {
		return TOKEN_TYPE_ERROR;
	}
	std::string escape_chars = "abfnrtv'\"?\\";

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
				char peeked_char = input->peek();
				if (escape_chars.find(peeked_char) == std::string::npos) {
					return TOKEN_TYPE_ERROR;
				}
				input->get();
				continue;
			}
			if (c == '\'') {
				char peeked_char = input->peek();
				// Allowing ; after quotes
				if (!(std::isspace(peeked_char) || peeked_char == ';')) {
					return TOKEN_TYPE_ERROR;
				}

				return TOKEN_TYPE_NORMAL;
			}
			continue;

		case TOKEN_STATE_DOUBLE_QUOTE:
			*value += c;
			if (c == '\\') {
				char peeked_char = input->peek();
				if (escape_chars.find(peeked_char) == std::string::npos) {
					return TOKEN_TYPE_ERROR;
				}
				input->get();
				continue;
			}
			if (c == '"') {
				char peeked_char = input->peek();
				// Allowing ; after quotes
				if (!(std::isspace(peeked_char) || peeked_char == ';')) {
					return TOKEN_TYPE_ERROR;
				}
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

bool NginxConfigParser::Parse(std::istream* config_file, NginxConfig* config) {
	if (config == nullptr) {
		return false;
	}

	std::stack<NginxConfig*> config_stack;
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
			NginxConfig* const new_config = new NginxConfig;
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

bool NginxConfigParser::Parse(const char* file_name, NginxConfig* config) {
	std::ifstream config_file;
	config_file.open(file_name);
	if (!config_file.is_open()) {
		std::cerr << "Failed to open config file: " << file_name << std::endl;
		return false;
	}

	const bool return_value =
	    Parse(dynamic_cast<std::istream*>(&config_file), config);
	config_file.close();
	return return_value;
}

int parse_args(int argc, const char* argv[], int* port) {
	if (argc != 2) {
		std::cerr << "Usage: webserver <config_file_path>\n";
		exit(1);
	}

	// Parse the config
	NginxConfigParser config_parser;
	NginxConfig config;
	if (!config_parser.Parse(argv[1], &config)) {
		std::cerr << "could not parse the config" << std::endl;
	}

	// Extract the port number from config
	*port = config.find_port();
	std::cerr << "found port number " << *port << std::endl;
	return 0;
}