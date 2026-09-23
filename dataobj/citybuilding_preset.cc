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

static std::string preset_path(const char *filename)
{
	return env_t::pak_dir + "config/" + filename;
}

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

void citybuilding_preset_load(vector_tpl<citybuilding_preset_t> &out)
{
	out.clear();
	tabfile_t file;
	const std::string path = preset_path("citybuilding_presets.tab");
	if (!file.open(path.c_str())) {
		return;
	}

	tabfileobj_t obj;
	while (file.read(obj)) {
		const char *raw_name = obj.get("name");
		if (!*raw_name) {
			continue;
		}
		citybuilding_preset_t preset;
		preset.name = tab_unescape(raw_name);
		char key[32];
		for (int i = 0; ; ++i) {
			snprintf(key, sizeof(key), "building[%d]", i);
			const char *building = obj.get(key);
			if (!*building) break;
			preset.buildings.push_back(tab_unescape(building));
		}
		if (preset.buildings.empty()) {
			dbg->warning("citybuilding_preset_load", "Preset \"%s\" has no buildings.", preset.name.c_str());
			continue;
		}
		bool replaced = false;
		for (uint32 i = 0; i < out.get_count(); ++i) {
			if (out[i].name == preset.name) {
				out[i] = preset;
				replaced = true;
				break;
			}
		}
		if (!replaced) out.append(preset);
	}
}

bool citybuilding_preset_save(const vector_tpl<citybuilding_preset_t> &presets)
{
	const std::string config_dir = env_t::pak_dir + "config/";
	if (dr_mkdir(config_dir.c_str()) != 0) {
		struct stat info;
		if (dr_stat(config_dir.c_str(), &info) != 0) {
			return false;
		}
	}

	const std::string path = preset_path("citybuilding_presets.tab");
	const std::string temporary = preset_path("citybuilding_presets.tab.tmp");
	FILE *file = dr_fopen(temporary.c_str(), "wb");
	if (!file) return false;

	for (uint32 i = 0; i < presets.get_count(); ++i) {
		if (i > 0) fputs("---\n", file);
		fprintf(file, "name=%s\n", tab_escape(presets[i].name).c_str());
		for (uint32 j = 0; j < presets[i].buildings.size(); ++j) {
			fprintf(file, "building[%u]=%s\n", j, tab_escape(presets[i].buildings[j]).c_str());
		}
	}
	if (fclose(file) != 0) {
		dr_remove(temporary.c_str());
		return false;
	}
	const std::string backup = preset_path("citybuilding_presets.tab.bak");
	FILE *existing = dr_fopen(path.c_str(), "rb");
	const bool has_existing = existing != NULL;
	if (existing) fclose(existing);
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
