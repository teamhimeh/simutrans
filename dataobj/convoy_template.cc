/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "convoy_template.h"
#include "tabfile.h"
#include "../utils/searchfolder.h"
#include "../simdebug.h"

std::vector<convoy_template_t> convoy_template_manager_t::s_templates;
bool convoy_template_manager_t::s_loaded = false;


static void parse_vehicle_list(const char *str, std::vector<std::string> &out)
{
	const char *p = str;
	while (*p) {
		while (*p == ' ' || *p == '\t') p++;
		if (!*p) break;
		const char *start = p;
		while (*p && *p != ',') p++;
		const char *end = p;
		while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
		if (end > start) {
			out.push_back(std::string(start, end - start));
		}
		if (*p == ',') p++;
	}
}


void convoy_template_manager_t::load(const std::string &pak_dir)
{
	s_templates.clear();
	s_loaded = true;

	std::string dir = pak_dir + "convoy_template/";
	searchfolder_t sf;
	if (sf.search(dir, "tab") == 0) {
		return;
	}

	for (searchfolder_t::const_iterator it = sf.begin(); it != sf.end(); ++it) {
		tabfile_t tf;
		if (!tf.open(*it)) {
			dbg->warning("convoy_template_manager_t::load", "Cannot open %s", *it);
			continue;
		}

		tabfileobj_t obj;
		while (tf.read(obj)) {
			const char *name = obj.get("name");
			if (!*name) continue;

			const char *veh_str = obj.get("vehicles");
			if (!*veh_str) continue;

			convoy_template_t tmpl;
			tmpl.name = name;
			parse_vehicle_list(veh_str, tmpl.vehicles);

			if (!tmpl.vehicles.empty()) {
				s_templates.push_back(tmpl);
			}
		}
	}

	dbg->message("convoy_template_manager_t::load", "Loaded %u convoy templates", (uint)s_templates.size());
}
