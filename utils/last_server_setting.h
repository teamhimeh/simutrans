/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */
#ifndef UTILS_LAST_SERVER_SETTING_H
#define UTILS_LAST_SERVER_SETTING_H

#include <string>

namespace last_server_setting {

inline bool parse(const std::string &line, std::string &address)
{
	const size_t eq = line.find('=');
	if(  eq == std::string::npos  ) { return false; }
	std::string key = line.substr(0, eq);
	const size_t first = key.find_first_not_of(" \t");
	if(  first == std::string::npos  ) { return false; }
	key = key.substr(first, key.find_last_not_of(" \t") - first + 1);
	if(  key != "# last_server"  ) { return false; }
	address = line.substr(eq + 1);
	const size_t begin = address.find_first_not_of(" \t\r\n");
	address = begin == std::string::npos ? "" : address.substr(begin, address.find_last_not_of(" \t\r\n") - begin + 1);
	return true;
}

inline std::string read(const std::string &contents)
{
	std::string result;
	for(  size_t start = 0; start < contents.size();  ) {
		const size_t newline = contents.find('\n', start);
		const size_t end = newline == std::string::npos ? contents.size() : newline + 1;
		std::string address;
		if(  parse(contents.substr(start, end - start), address)  ) { result = address; }
		start = end;
	}
	return result;
}

inline bool update(std::string &contents, const std::string &address)
{
	if(  address.find_first_of("\r\n") != std::string::npos  ) { return false; }
	const char *eol = contents.find("\r\n") == std::string::npos ? "\n" : "\r\n";
	std::string preserved;
	for(  size_t start = 0; start < contents.size();  ) {
		const size_t newline = contents.find('\n', start);
		const size_t end = newline == std::string::npos ? contents.size() : newline + 1;
		const std::string line = contents.substr(start, end - start);
		std::string unused;
		if(  !parse(line, unused)  ) { preserved += line; }
		start = end;
	}
	if(  !preserved.empty() && preserved.back() != '\n'  ) { preserved += eol; }
	contents = preserved + "# last_server = " + address + eol;
	return true;
}

}
#endif
