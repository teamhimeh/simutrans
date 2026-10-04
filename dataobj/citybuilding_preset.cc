/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "citybuilding_preset.h"
#include "environment.h"
#include "tabfile.h"

#include "../simdebug.h"
#include "../sys/simsys.h"

#include <stdio.h>
#include <sys/stat.h>

static std::string tab_escape(const std::string &value)
{
	std::string result;
	for (std::string::const_iterator i = value.begin(); i != value.end(); ++i) {
		const unsigned char c = (unsigned char)*i;
		if (c == '%' || c == '<' || c == '>' || c == '\r' || c == '\n') {
			char encoded[4];
			snprintf(encoded, sizeof(encoded), "%%%02X", c);
			result += encoded;
		}
		else {
			result += *i;
		}
	}
	return result;
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

static std::string tab_unescape(const char *value)
{
	std::string result;
	for (const char *i = value; *i; ++i) {
		if (*i == '%' && i[1] && i[2]) {
			const int hi = hex_value(i[1]);
			const int lo = hex_value(i[2]);
			if (hi >= 0 && lo >= 0) {
				result += (char)((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		result += *i;
	}
	return result;
}

bool citybuilding_preset_load(const std::string &path, citybuilding_preset_t &out)
{
	tabfile_t file;
	if (!file.open(path.c_str())) return false;
	tabfileobj_t obj;
	if (!file.read(obj)) return false;
	citybuilding_preset_t preset;
	preset.name = tab_unescape(obj.get("name"));
	preset.pakset = tab_unescape(obj.get("pakset"));
	if (preset.name.empty()) return false;
	char key[32];
	for (int i = 0; ; ++i) {
		snprintf(key, sizeof(key), "building[%d]", i);
		const char *building = obj.get(key);
		if (!*building) break;
		preset.buildings.push_back(tab_unescape(building));
	}
	if (preset.buildings.empty() || file.read(obj)) return false;
	out = preset;
	return true;
}

bool citybuilding_preset_save(const std::string &path, const citybuilding_preset_t &preset)
{
	const std::string preset_dir = env_t::pak_dir + "building_preset/";
	if (dr_mkdir(preset_dir.c_str()) != 0) {
		struct stat info;
		if (dr_stat(preset_dir.c_str(), &info) != 0 || (info.st_mode & S_IFMT) != S_IFDIR) {
			return false;
		}
	}

	const std::string temporary = path + ".tmp";
	FILE *file = dr_fopen(temporary.c_str(), "wb");
	if (!file) return false;

	fprintf(file, "name=%s\n", tab_escape(preset.name).c_str());
	fprintf(file, "pakset=%s\n", tab_escape(preset.pakset).c_str());
	for (uint32 j = 0; j < preset.buildings.size(); ++j) {
		fprintf(file, "building[%u]=%s\n", j, tab_escape(preset.buildings[j]).c_str());
	}
	const bool write_failed = ferror(file) != 0;
	if (fclose(file) != 0 || write_failed) {
		dr_remove(temporary.c_str());
		return false;
	}
	const std::string backup = path + ".bak";
	struct stat existing;
	const bool has_existing = dr_stat(path.c_str(), &existing) == 0;
	struct stat backup_info;
	if (has_existing && dr_stat(backup.c_str(), &backup_info) == 0) {
		// Preserve a backup left by an interrupted or failed replacement.
		dr_remove(temporary.c_str());
		return false;
	}
	if (has_existing && dr_rename(path.c_str(), backup.c_str()) != 0) {
		dr_remove(temporary.c_str());
		return false;
	}
	if (dr_rename(temporary.c_str(), path.c_str()) != 0) {
		if (has_existing) dr_rename(backup.c_str(), path.c_str());
		dr_remove(temporary.c_str());
		return false;
	}
	if (has_existing) dr_remove(backup.c_str());
	return true;
}
