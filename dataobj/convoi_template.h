/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_CONVOI_TEMPLATE_H
#define DATAOBJ_CONVOI_TEMPLATE_H

#include <string>
#include <vector>

struct convoi_template_t {
	std::string name;
	std::vector<std::string> vehicles; // pak descriptor names
};

class convoi_template_manager_t {
	static std::vector<convoi_template_t> s_templates;
	static bool s_loaded;
public:
	static void load(const std::string &pak_dir);
	static bool is_loaded() { return s_loaded; }
	static const std::vector<convoi_template_t> &get_templates() { return s_templates; }
};

#endif
