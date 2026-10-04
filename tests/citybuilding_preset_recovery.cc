// Standalone recovery regression test. Uses the real preset and tab parsers,
// with injectable filesystem failures; no running game or Pakset is needed.
// g++ -std=c++17 -ffunction-sections -fdata-sections -Wl,--gc-sections 
// tests/citybuilding_preset_recovery.cc dataobj/citybuilding_preset.cc 
// dataobj/tabfile.cc utils/simstring.cc -o build/preset-recovery-test.exe
#include "../dataobj/citybuilding_preset.h"
#include "../dataobj/environment.h"
#include "../simdebug.h"
#include "../dataobj/freelist.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <errno.h>
#include <sys/stat.h>
#include <stdexcept>

namespace fs = std::filesystem;
std::string env_t::pak_dir;
log_t *dbg = NULL;
void *freelist_t::gimme_node(size_t size) { return malloc(size); }
void freelist_t::putback_node(size_t, void *node) { free(node); }
uint16 get_system_color(unsigned int, unsigned int, unsigned int) { return 0; }
uint32 get_color_rgb(uint8) { return 0; }
uint16 color_idx_to_rgb(uint16) { return 0; }
void log_t::warning(const char *, const char *, ...) {}
void log_t::error(const char *, const char *, ...) {}
void log_t::message(const char *, const char *, ...) {}
void log_t::fatal(const char *, const char *, ...) { abort(); }

static std::string fail_stat, fail_remove, fail_rename, fail_open, fail_rename_second;
static fs::path native(const std::string &s) { return fs::u8path(s); }
FILE *dr_fopen(const char *path, const char *mode)
{
	if (path == fail_open) { errno = EACCES; return NULL; }
#ifdef _WIN32
	return _wfopen(native(path).c_str(), native(mode).c_str());
#else
	return fopen(path, mode);
#endif
}
int dr_stat(const char *path, struct stat *info)
{
	if (path == fail_stat) { errno = EACCES; return -1; }
#ifdef _WIN32
	return _wstat(native(path).c_str(), (struct _stat*)info);
#else
	return stat(path, info);
#endif
}
int dr_remove(const char *path)
{
	if (path == fail_remove) { errno = EACCES; return -1; }
	std::error_code error;
	if (fs::remove(native(path), error)) return 0;
	errno = error ? EACCES : ENOENT;
	return -1;
}
int dr_rename(const char *source, const char *target)
{
	if (source == fail_rename || source == fail_rename_second) { errno = EACCES; return -1; }
	std::error_code error;
	fs::rename(native(source), native(target), error);
	if (!error) return 0;
	errno = EACCES;
	return -1;
}
int dr_mkdir(const char *path)
{
	std::error_code error;
	if (fs::create_directory(native(path), error)) return 0;
	errno = error ? EACCES : EEXIST;
	return -1;
}
static void require(bool ok) { if (!ok) throw std::runtime_error("assertion failed"); }
static void put(const std::string &path, const std::string &contents)
{
	std::ofstream file(native(path), std::ios::binary);
	file << contents;
	require(file.good());
}
int main(int argc, char **argv)
{
	if (argc != 2) return 2; // Caller supplies an empty disposable directory.
	const fs::path root = native(argv[1]);
	require(fs::is_directory(root) && fs::is_empty(root));
	env_t::pak_dir = root.u8string() + "/";
	fs::create_directory(root / "building_preset");
	const std::string prefix = env_t::pak_dir + "building_preset/";
	const std::string valid = "building[0]=COM_00_01\n";
	int count = 0;
	for (int target = 0; target < 3; ++target) {
		for (int backup = 0; backup < 3; ++backup) {
			for (int temporary = 0; temporary < 2; ++temporary) {
				const std::string path = prefix + std::to_string(count++) + ".tab";
				if (target) put(path, target == 1 ? valid : "invalid");
				if (backup) put(path + ".bak", backup == 1 ? valid : "invalid");
				if (temporary) put(path + ".tmp", "partial");
				std::string archived;
				const auto result = citybuilding_preset_recover(path, &archived);
				if (backup == 2 && target != 1) {
					require(result == CITYBUILDING_PRESET_RECOVERY_FAILED);
					require(fs::exists(native(path + ".bak")));
					require(fs::exists(native(path + ".tmp")) == bool(temporary));
				}
				else {
					require(result == ((!backup && !temporary) ? CITYBUILDING_PRESET_UNCHANGED : CITYBUILDING_PRESET_RECOVERED));
					require(!fs::exists(native(path + ".bak")) && !fs::exists(native(path + ".tmp")));
					require(fs::exists(native(path)) == bool(target || backup));
					require(!archived.empty() == bool(target == 2 && backup));
					require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_UNCHANGED);
				}
			}
		}
	}
	std::string path = prefix + u8"日本語.tab";
	put(path, "invalid"); put(path + ".bak", valid);
	put(path + ".corrupt", "keep"); put(path + ".corrupt.1", "keep");
	std::string archived;
	require(citybuilding_preset_recover(path, &archived) == CITYBUILDING_PRESET_RECOVERED);
	require(archived == path + ".corrupt.2");
	citybuilding_preset_t loaded;
	require(citybuilding_preset_load(path, loaded) && loaded.name.empty() && loaded.buildings[0] == "COM_00_01");
	loaded.name = u8"日本語%<>";
	loaded.buildings.push_back(u8"建物%<>\r\n");
	require(citybuilding_preset_save(path, loaded));
	citybuilding_preset_t roundtrip;
	require(citybuilding_preset_load(path, roundtrip) && roundtrip.name.empty() && roundtrip.buildings == loaded.buildings);
	std::ifstream saved(native(path), std::ios::binary);
	const std::string contents((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
	require(contents.find("name=") == std::string::npos);
	saved.close();
	put(path, "name=ignored\n" + valid);
	require(citybuilding_preset_load(path, roundtrip) && roundtrip.name.empty() && roundtrip.buildings[0] == "COM_00_01");
	put(path, "name=ignored\n");
	require(!citybuilding_preset_load(path, roundtrip));
	for (int fault = 0; fault < 4; ++fault) {
		path = prefix + "fault" + std::to_string(fault) + ".tab";
		put(path, "invalid"); put(path + ".bak", valid); put(path + ".tmp", "partial");
		if (fault == 0) fail_stat = path;
		if (fault == 1) fail_remove = path + ".tmp";
		if (fault == 2) fail_rename = path + ".bak";
		if (fault == 3) fail_rename = path;
		require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_RECOVERY_FAILED);
		require(fs::exists(native(path)));
		if (fault != 1) require(fs::exists(native(path + ".bak")));
		fail_stat.clear(); fail_remove.clear(); fail_rename.clear();
		require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_RECOVERED);
		require(citybuilding_preset_load(path, loaded) && loaded.buildings[0] == "COM_00_01");
	}
	path = prefix + "directory.tab";
	fs::create_directory(native(path)); put(path + ".tmp", "partial");
	require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_RECOVERY_FAILED);
	require(fs::exists(native(path + ".tmp")));
	path = prefix + "rollback-failure.tab";
	put(path, "invalid"); put(path + ".bak", valid);
	fail_rename = path + ".bak";
	fail_rename_second = path + ".corrupt";
	require(citybuilding_preset_recover(path, &archived) == CITYBUILDING_PRESET_RECOVERY_FAILED);
	require(archived == path + ".corrupt");
	require(fs::exists(native(archived)) && fs::exists(native(path + ".bak")));
	fail_rename.clear(); fail_rename_second.clear();
	require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_RECOVERED);
	path = prefix + "backup-remove-failure.tab";
	put(path, valid); put(path + ".bak", valid);
	fail_remove = path + ".bak";
	require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_RECOVERY_FAILED);
	require(fs::exists(native(path)) && fs::exists(native(path + ".bak")));
	fail_remove.clear();
	require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_RECOVERED);
	path = prefix + "write-failure.tab";
	put(path, valid);
	fail_open = path + ".tmp";
	require(!citybuilding_preset_save(path, loaded));
	fail_open.clear();
	require(citybuilding_preset_load(path, loaded) && loaded.buildings[0] == "COM_00_01");
	env_t::pak_dir = (root / "new-pak").u8string() + "/";
	fs::create_directory(native(env_t::pak_dir));
	path = env_t::pak_dir + "building_preset/new.tab";
	require(citybuilding_preset_recover(path) == CITYBUILDING_PRESET_UNCHANGED);
	require(citybuilding_preset_save(path, loaded));
	require(citybuilding_preset_load(path, roundtrip) && roundtrip.buildings == loaded.buildings);
	std::cout << "PASS: 18 recovery states, quarantine collision, Japanese roundtrip, 4 injected failures and retry, non-regular target\n";
	std::cout << "PASS: rollback failure and retry, backup removal failure, write failure preservation, missing directory/new save\n";
}
