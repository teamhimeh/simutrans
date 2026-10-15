// Standalone test: g++ -std=c++11 tests/serverlist-edit-test.cc -o build/serverlist-edit-test.exe
#include "../utils/serverlist_edit.h"
#include <cassert>

int main()
{
	using namespace serverlist_edit;
	std::string name, address;
	assert(!parse("# comment = ignored", name, address));
	assert(!parse("empty =  \r\n", name, address));
	assert(parse(" 日本語 = [::1]:13353\r\n", name, address));
	assert(name == "日本語" && address == "[::1]:13353");
	std::string contents = "# Keep this\r\n\r\nNamed server = localhost\r\ninvalid line";
	const std::string original = contents;
	assert(!edit(contents, "", "localhost"));
	assert(contents == original);
	assert(edit(contents, "", "[::1]:13354"));
	assert(contents == original + "\r\n[::1]:13354 = [::1]:13354\r\n");
	assert(edit(contents, "Named server", "localhost"));
	assert(contents == "# Keep this\r\n\r\ninvalid line\r\n[::1]:13354 = [::1]:13354\r\n");
	const std::string saved = contents;
	assert(!edit(contents, "Other name", "[::1]:13354"));
	assert(contents == saved);
	assert(!edit(contents, "", ""));
	assert(!edit(contents, "", "host\nInjected = host"));
	assert(!edit(contents, "", "host = other"));
	contents.clear();
	assert(edit(contents, "", "192.0.2.1:13353"));
	assert(contents == "192.0.2.1:13353 = 192.0.2.1:13353\n");
	assert(edit(contents, "192.0.2.1:13353", "192.0.2.1:13353"));
	assert(contents.empty());
	contents = "same = host\nother = host\n";
	assert(edit(contents, "same", "host"));
	assert(contents == "other = host\n");
	contents = "last = host";
	assert(edit(contents, "last", "host"));
	assert(contents.empty());
	// Long lines are parsed as a whole rather than as separate registrations.
	contents = "#" + std::string(4096, 'x') + "\nname = host\n";
	assert(edit(contents, "name", "host"));
	assert(contents == "#" + std::string(4096, 'x') + "\n");
}
