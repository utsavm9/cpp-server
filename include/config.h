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

	// To be called after extracting all relevant information
	// Empties the statements_ vector assuming raw config is no longer needed
	void free_memory();

	// Extracts the port number it finds in the stored config,
	// otherwise returns the default port 80.
	// Searches for "server {port <port_num>;}" or "port <port_num>;" in config. Logs invalid entries.
	int get_port();

	// Extracts the actions to do from the prefix of request targets
	// Supports "echo" action, default is "/"
	// Supports "static" action, default is "/"
	void extract_targets();

	// Mapping target url paths to potential handlers
	std::vector<std::pair<std::string, std::string>> urlToServiceName;

	// Hashmap mapping url paths to filesystem
	std::unordered_map<std::string, std::string> urlToLinux;
};
