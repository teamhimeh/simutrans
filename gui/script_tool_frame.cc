/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simdebug.h"
#include "../simsys.h"

#include "script_tool_frame.h"
#include "messagebox.h"

#include "simwin.h"
#include "../simworld.h"
#include "../simmenu.h"
#include "../simtool.h"

#include "../dataobj/environment.h"
#include "../dataobj/translator.h"

#include "../network/network.h"
#include "../network/network_cmd.h"

#include "../utils/cbuffer_t.h"

script_tool_frame_t::script_tool_frame_t() : savegame_frame_t(NULL, true, NULL, false)
{
	static cbuffer_t pakset_script_tool;
	static cbuffer_t addons_script_tool;

	pakset_script_tool.clear();
	pakset_script_tool.printf("%stool/", env_t::program_dir, env_t::objfilename.c_str());

	addons_script_tool.clear();
	addons_script_tool.printf("%stool/", env_t::objfilename.c_str());

	if (env_t::default_settings.get_with_private_paks()) {
		this->add_path(addons_script_tool);
	}
	this->add_path(pakset_script_tool);

	set_name(translator::translate("Load script_tool"));
	set_focus(NULL);
}


/**
 * Action, started after button pressing.
 */
bool script_tool_frame_t::item_action(const char *fullpath)
{
	tool_exec_script_t* tool = static_cast<tool_exec_script_t*>(tool_t::general_tool[TOOL_EXEC_SCRIPT]);
	welt->set_tool( tool, welt->get_active_player() );
	tool->load_script(fullpath);
	return true;
}


const char *script_tool_frame_t::get_info(const char *filename)
{
	static char info[PATH_MAX];

	sprintf(info,"%s",this->get_filename(filename, false).c_str());

	return info;
}


bool script_tool_frame_t::check_file( const char *filename, const char * )
{
	char buf[PATH_MAX];
	sprintf( buf, "%s/tool.nut", filename );
	if (FILE* const f = dr_fopen(buf, "r")) {
		fclose(f);
		return true;
	}
	return false;
}
