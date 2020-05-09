/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef BAUER_SCRIPT_TOOL_MANAGER_H
#define BAUER_SCRIPT_TOOL_MANAGER_H


#include "../dataobj/koord3d.h"
#include "../simtypes.h"
#include "../tpl/vector_tpl.h"

class tool_selector_t;
class tool_exec_script_t;

/**
 * There's no need to construct an instance since everything is static here.
 */
class script_tool_manager_t
{
private:
	static vector_tpl<tool_exec_script_t*> script_tools;  ///< All script tools

public:
	static void load_scripts(char const* path);
	
	static bool check_file(char const* path);
	
	static void fill_menu(tool_selector_t* tool_selector, char const* arg, sint16 sound_ok);
};

#endif
