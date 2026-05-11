/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "convoi_template.h"
#include "tabfile.h"
#include "../utils/searchfolder.h"
#include "../simdebug.h"

std::vector<convoi_template_t> convoi_template_manager_t::s_templates;
bool convoi_template_manager_t::s_loaded = false;


static void load_from_dir(const std::string &dir, std::vector<convoi_template_t> &out)
{
	searchfolder_t sf;
	if (sf.search(dir, "tab") == 0) {
		return;
	}
	for (searchfolder_t::const_iterator it = sf.begin(); it != sf.end(); ++it) {
		tabfile_t tf;
		if (!tf.open(*it)) {
			dbg->warning("convoi_template_manager_t::load", "Cannot open %s", *it);
			continue;
		}
		tabfileobj_t obj;
		while (tf.read(obj)) {
			const char *name = obj.get("name");
			if (!*name) continue;

			convoi_template_t tmpl;
			tmpl.name = name;

			// vehicle[0], vehicle[1], ... — one descriptor name per entry
			char key[32];
			for (int i = 0; ; i++) {
				snprintf(key, sizeof(key), "vehicle[%d]", i);
				const char *veh = obj.get(key);
				if (!*veh) break;
				tmpl.vehicles.push_back(veh);
			}

			if (!tmpl.vehicles.empty()) {
				out.push_back(tmpl);
			} else {
				dbg->error("convoi_template_manager_t::load", "Convoy template \"%s\" has no vehicle entries.", tmpl.name.c_str());
			}
		}
	}
}


void convoi_template_manager_t::load(const std::string &pak_dir)
{
	s_templates.clear();
	s_loaded = true;

	load_from_dir(pak_dir + "convoy_template/", s_templates);
	load_from_dir(pak_dir + "addons/convoy_template/", s_templates);

	dbg->message("convoi_template_manager_t::load", "Loaded %u convoy templates", (uint)s_templates.size());
}
