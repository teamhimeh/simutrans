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

// Read and write presets in the config directory of the current pakset.
void citybuilding_preset_load(vector_tpl<citybuilding_preset_t> &out);
bool citybuilding_preset_save(const vector_tpl<citybuilding_preset_t> &presets);

#endif
