/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

// Checks the production validator utils/save_filename.cc for savegame names chosen by scripts.

#include "utils/save_filename.h"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void expect(const std::string &name, bool valid, const char *what)
{
	std::string error;
	const bool result = is_valid_script_save_name(name, error);
	if(  result != valid  ||  (!result  &&  error.empty())  ) {
		printf("FAIL: %s (%s): expected %s, got %s\n", what, name.c_str(), valid ? "valid" : "invalid", result ? "valid" : "invalid");
		failures++;
	}
}

#define OK(name, what) expect(std::string(name), true, what)
#define NG(name, what) expect(std::string(name), false, what)

int main()
{
	// accepted
	OK("mysave", "plain");
	OK("my save 01", "inner spaces");
	OK("my-save_1.v2", "dash, underscore, inner dot");
	OK("\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88", "Japanese");
	OK("\xF0\x9F\x9A\x83" "train", "4 byte character");
	OK("a\xCC\x81", "combining mark");
	OK("comx", "not a device name");
	OK("com10", "not a device name");
	OK("console", "not a device name");
	OK(std::string(SCRIPT_SAVE_NAME_MAX_BYTES, 'a'), "longest name");

	// empty and length
	NG("", "empty");
	NG(std::string(SCRIPT_SAVE_NAME_MAX_BYTES + 1, 'a'), "too long");

	// directory traversal and separators
	NG("..", "parent");
	NG(".", "current");
	NG("../evil", "parent traversal");
	NG("..\\evil", "windows parent traversal");
	NG("a/b", "slash");
	NG("a\\b", "backslash");
	NG("/etc/passwd", "absolute posix");
	NG("C:evil", "drive relative");
	NG("C:\\evil", "drive absolute");
	NG("\\\\server\\share\\x", "unc");
	NG("a/../../b", "embedded traversal");
	NG(".hidden", "leading dot");
	NG("~/x", "home");
	OK("~x", "tilde alone is a normal character");

	// characters forbidden on Windows
	const char forbidden[] = ":*?\"<>|";
	for(  const char *c = forbidden; *c; c++  ) {
		NG(std::string("a") + *c + "b", "forbidden character");
	}
	NG("name ", "trailing space");
	NG(" name", "leading space");
	NG("name.", "trailing dot");

	// Windows device names, any case, any extension
	const char *devices[] = { "CON", "con", "Prn", "AUX", "nul", "COM1", "com9", "LPT1", "lpt9", "COM0", "CONIN$", "CONOUT$" };
	for(  size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++  ) {
		NG(devices[i], "device name");
		NG(std::string(devices[i]) + ".txt", "device name with extension");
		NG(std::string(devices[i]) + " .txt", "device name with space");
	}

	// control characters
	for(  int c = 1; c < 0x20; c++  ) {
		NG(std::string("a") + (char)c + "b", "C0 control");
	}
	NG(std::string("a\0b", 3), "embedded NUL");
	NG("a\x7F" "b", "DEL");
	NG("a\xC2\x85" "b", "C1 control NEL");
	NG("a\xC2\x9F" "b", "C1 control");
	NG("a\n", "newline");

	// invisible and direction changing characters
	NG("a\xE2\x80\xAE" "b", "right-to-left override");
	NG("a\xE2\x80\x8B" "b", "zero width space");
	NG("a\xE2\x80\xA8" "b", "line separator");
	NG("a\xEF\xBB\xBF" "b", "BOM");
	NG("a\xC2\xAD" "b", "soft hyphen");
	NG("a\xEE\x80\x80" "b", "private use");
	NG("a\xEF\xBF\xBE" "b", "noncharacter");

	// invalid UTF-8
	NG("a\x80" "b", "lone continuation byte");
	NG("a\xC3", "truncated 2 byte");
	NG("a\xE3\x83", "truncated 3 byte");
	NG("a\xC0\xAF", "overlong slash");
	NG("a\xE0\x80\xAF", "overlong slash 3 byte");
	NG("a\xED\xA0\x80", "surrogate");
	NG("a\xF4\x90\x80\x80", "above U+10FFFF");
	NG("a\xF8\x88\x80\x80\x80", "5 byte form");
	NG("a\xFF", "0xFF");

	// invariant: nothing accepted can leave the save folder, whatever the input
	std::vector<std::string> alphabet;
	const char *pieces[] = { "a", ".", "..", "/", "\\", ":", " ", "\xC2\xAD", "\xE2\x80\xAE", "\xC3\xA9", "\xE3\x83\x86", "%2e", "~" };
	for(  size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); i++  ) {
		alphabet.push_back(pieces[i]);
	}
	std::vector<std::string> level(1, "");
	for(  int depth = 0; depth < 4; depth++  ) {
		std::vector<std::string> next;
		for(  size_t i = 0; i < level.size(); i++  ) {
			for(  size_t j = 0; j < alphabet.size(); j++  ) {
				const std::string s = level[i] + alphabet[j];
				next.push_back(s);
				std::string error;
				if(  is_valid_script_save_name(s, error)  ) {
					if(  s.find_first_of("/\\:") != std::string::npos  ||  s[0] == '.'  ) {
						printf("FAIL: unsafe name accepted: %s\n", s.c_str());
						failures++;
					}
				}
			}
		}
		level.swap(next);
	}

	if(  failures  ) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("All save name checks passed\n");
	return 0;
}
