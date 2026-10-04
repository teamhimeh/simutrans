/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_CITYBUILDING_PRESET_H
#define DATAOBJ_CITYBUILDING_PRESET_H

#include <string>
#include <vector>

#include "../tpl/vector_tpl.h"

struct citybuilding_preset_t {
	std::string name;
	std::vector<std::string> buildings;
};

// Read and safely replace a single preset file.
bool citybuilding_preset_load(const std::string &path, citybuilding_preset_t &out);
bool citybuilding_preset_save(const std::string &path, const citybuilding_preset_t &preset);

#endif
