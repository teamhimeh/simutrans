/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "save_filename.h"

#include <cstring>


/// Decodes one UTF-8 sequence. Rejects overlong forms, surrogates and values above U+10FFFF.
static bool decode_utf8(const std::string &s, size_t &pos, unsigned long &cp)
{
	const unsigned char c = (unsigned char)s[pos];
	size_t len;
	unsigned long min;
	if(  c < 0x80  ) { cp = c; len = 1; min = 0; }
	else if(  (c & 0xE0) == 0xC0  ) { cp = c & 0x1F; len = 2; min = 0x80; }
	else if(  (c & 0xF0) == 0xE0  ) { cp = c & 0x0F; len = 3; min = 0x800; }
	else if(  (c & 0xF8) == 0xF0  ) { cp = c & 0x07; len = 4; min = 0x10000; }
	else { return false; }

	if(  pos + len > s.size()  ) {
		return false;
	}
	for(  size_t i = 1; i < len; i++  ) {
		const unsigned char t = (unsigned char)s[pos + i];
		if(  (t & 0xC0) != 0x80  ) {
			return false;
		}
		cp = (cp << 6) | (t & 0x3F);
	}
	if(  cp < min  ||  cp > 0x10FFFF  ||  (cp >= 0xD800  &&  cp <= 0xDFFF)  ) {
		return false;
	}
	pos += len;
	return true;
}


/// Control characters and characters that change how text is shown or hide it.
static bool is_unsafe_code_point(unsigned long cp)
{
	return cp < 0x20  ||  cp == 0x7F  ||  (cp >= 0x80  &&  cp <= 0x9F)  // C0, DEL, C1
		|| cp == 0x00AD                                              // soft hyphen
		|| (cp >= 0x200B  &&  cp <= 0x200F)                          // zero width, LRM, RLM
		|| (cp >= 0x2028  &&  cp <= 0x202E)                          // line/paragraph separator, bidi
		|| (cp >= 0x2060  &&  cp <= 0x206F)                          // word joiner, bidi isolates
		|| cp == 0xFEFF                                              // BOM
		|| (cp >= 0xFFF9  &&  cp <= 0xFFFB)                          // interlinear annotation
		|| (cp >= 0xE000  &&  cp <= 0xF8FF)                          // private use
		|| (cp >= 0xFFFE  &&  cp <= 0xFFFF)                          // noncharacters
		|| (cp >= 0xE0000  &&  cp <= 0xE0FFF);                       // tags, variation selectors supplement
}


/// ASCII case-insensitive equality (locale independent, available on every compiler).
static bool equals_nocase(const std::string &a, const char *b)
{
	const size_t n = strlen(b);
	if(  a.size() != n  ) {
		return false;
	}
	for(  size_t i = 0; i < n; i++  ) {
		char c = a[i];
		if(  c >= 'a'  &&  c <= 'z'  ) {
			c = (char)(c - 'a' + 'A');
		}
		if(  c != b[i]  ) {
			return false;
		}
	}
	return true;
}


static bool is_windows_device_name(const std::string &name)
{
	// the device name counts with any extension and any trailing spaces: "con.txt", "NUL .x"
	size_t end = name.find('.');
	if(  end == std::string::npos  ) {
		end = name.size();
	}
	while(  end > 0  &&  name[end - 1] == ' '  ) {
		end--;
	}
	const std::string base = name.substr(0, end);

	static const char *const plain[] = { "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$", "CLOCK$" };
	for(  size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); i++  ) {
		if(  equals_nocase(base, plain[i])  ) {
			return true;
		}
	}
	if(  base.size() == 4  &&  (equals_nocase(base.substr(0, 3), "COM")  ||  equals_nocase(base.substr(0, 3), "LPT"))  ) {
		return base[3] >= '0'  &&  base[3] <= '9';
	}
	return false;
}


bool is_valid_script_save_name(const std::string &name, std::string &error)
{
	if(  name.empty()  ) {
		error = "name is empty";
		return false;
	}
	if(  name.size() > SCRIPT_SAVE_NAME_MAX_BYTES  ) {
		error = "name is too long";
		return false;
	}

	for(  size_t pos = 0; pos < name.size();  ) {
		unsigned long cp;
		if(  !decode_utf8(name, pos, cp)  ) {
			error = "name is not valid UTF-8";
			return false;
		}
		if(  is_unsafe_code_point(cp)  ) {
			error = "name contains a control or invisible character";
			return false;
		}
		if(  cp < 0x80  &&  strchr("/\\:*?\"<>|", (int)cp)  ) {
			error = "name contains a character that is not allowed in file names";
			return false;
		}
	}

	if(  name[0] == '.'  ) {
		error = "name must not start with a dot";
		return false;
	}
	if(  name[0] == ' '  ||  name[name.size() - 1] == ' '  ||  name[name.size() - 1] == '.'  ) {
		error = "name must not start or end with a space, or end with a dot";
		return false;
	}
	if(  is_windows_device_name(name)  ) {
		error = "name is reserved by the operating system";
		return false;
	}
	return true;
}
