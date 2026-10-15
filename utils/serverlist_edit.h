/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */
#ifndef UTILS_SERVERLIST_EDIT_H
#define UTILS_SERVERLIST_EDIT_H

#include <string>

namespace serverlist_edit {

inline std::string trim(const std::string &s)
{
	const size_t first = s.find_first_not_of(" \t\r\n");
	return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

inline bool parse(const std::string &line, std::string &name, std::string &address)
{
	const std::string text = trim(line);
	if(  text.empty()  ||  text[0] == '#'  ) { return false; }
	const size_t eq = text.find('=');
	if(  eq == std::string::npos  ) { return false; }
	name = trim(text.substr(0, eq));
	address = trim(text.substr(eq + 1));
	return !name.empty() && !address.empty();
}

inline bool valid_address(const std::string &address)
{
	return !address.empty() && address.find_first_of(" \t\r\n=#") == std::string::npos;
}

// Preserve comments, line endings and unrelated entries byte for byte.
// An empty name adds an address; otherwise remove the matching saved entry.
inline bool edit(std::string &contents, const std::string &name, const std::string &address)
{
	if(  name.empty() && !valid_address(address)  ) { return false; }
	for(  size_t start = 0; start < contents.size();  ) {
		const size_t newline = contents.find('\n', start);
		const size_t end = newline == std::string::npos ? contents.size() : newline + 1;
		std::string saved_name, saved_address;
		if(  parse(contents.substr(start, end - start), saved_name, saved_address) && saved_address == address  ) {
			if(  name.empty()  ) { return false; } // Already registered.
			if(  saved_name == name  ) {
				contents.erase(start, end - start);
				return true;
			}
		}
		start = end;
	}
	if(  !name.empty()  ) { return false; }
	const char *eol = contents.find("\r\n") == std::string::npos ? "\n" : "\r\n";
	if(  !contents.empty() && contents.back() != '\n'  ) { contents += eol; }
	contents += address + " = " + address + eol;
	return true;
}

}
#endif
