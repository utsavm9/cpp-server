// An nginx config.

#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class NginxConfig;

// The parsed representation of a single config statement.
class NginxConfigStatement {
   public:
	std::string ToString(int depth);
	std::vector<std::string> tokens_;
	std::unique_ptr<NginxConfig> child_block_;
};

// The parsed representation of the entire config.
class NginxConfig {
   public:
	// Fills in default values for information
	// that can also be extracted.
	NginxConfig();

	std::string ToString(int depth = 0);
	std::vector<std::shared_ptr<NginxConfigStatement>> statements_;

	// Extracts integer and strings from fields in the config
	int get_num(std::string field);
	std::string get_str(std::string field);

	static std::unordered_map<std::string, short> default_nums;
};
