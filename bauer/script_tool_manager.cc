/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "script_tool_manager.h"

#include "../dataobj/tabfile.h"

#include "../simdebug.h"
#include "../simtool.h"
#include "../simskin.h"
#include "../sys/simsys.h"

#include "../gui/tool_selector.h"
#include "../utils/searchfolder.h"


vector_tpl<tool_exec_script_t*> script_tool_manager_t::one_click_script_tools;
vector_tpl<tool_exec_two_click_script_t*> script_tool_manager_t::two_click_script_tools;

bool script_tool_manager_t::check_file( const char *filename )
{
	char buf[PATH_MAX];
	sprintf( buf, "%s/tool.nut", filename );
	if (FILE* const f = dr_fopen(buf, "r")) {
		fclose(f);
		return true;
	}
	return false;
}

bool script_tool_manager_t::is_two_click_tool( const char *filename ) {
	// read description.tab
	char buf[PATH_MAX];
	sprintf( buf, "%s/description.tab", filename );
	tabfile_t file;
	if (  !file.open(buf)  ) {
		// no description.tab -> handle as one_click script tool
		return false;
	}
	tabfileobj_t contents;
	file.read( contents );
	return strcmp(contents.get_string("type", "one_click"), "two_click")==0;
}

void script_tool_manager_t::load_tool(tool_t* ttl, exec_script_base_t* etl, char const* fullpath, char const* name) {
	// open description.tab and get more info
	char buf[PATH_MAX];
	sprintf( buf, "%s/description.tab", fullpath );
	tabfile_t file;
	if (  !file.open(buf)  ) {
		// no description.tab -> handle as one_click script tool
		// set only default_param and title
		ttl->set_default_param(fullpath);
		etl->set_title(name);
		return;
	}
	
	tabfileobj_t contents;
	file.read( contents );
	ttl->set_default_param(fullpath);
	etl->set_title(contents.get_string("title", ttl->get_default_param()));
	etl->set_menu_arg(contents.get_string("menu", ttl->get_default_param()));
	if(  contents.get_int("restart", 1) > 0  ) {
		etl->enable_restart();
	}
	const char* cursor_name = contents.get_string("icon", "-");
	const skin_desc_t * desc = skinverwaltung_t::get_extra(cursor_name, strlen(cursor_name), skinverwaltung_t::cursor);
	if(  desc  ) {
		ttl->cursor = desc->get_image_id(0);
		ttl->set_icon(desc->get_image_id(1));
	}
	return;
}

void script_tool_manager_t::load_scripts(char const* path) {
  dbg->message("script_tool_manager_t::load_scripts", "Loading scripts from %s", path);
  searchfolder_t find;
  find.search(path, "", true, false);
  FOR(searchfolder_t, const &name, find) {
    char* fullname = new char [strlen(path)+strlen(name)+1];
    sprintf(fullname,"%s%s",path,name);
		
		if(  is_two_click_tool(fullname)  ) {
			tool_exec_two_click_script_t* tt = new tool_exec_two_click_script_t();
			load_tool(tt, tt, fullname, name);
			two_click_script_tools.append(tt);
		} else {
			tool_exec_script_t* ot = new tool_exec_script_t();
			load_tool(ot, ot, fullname, name);
			one_click_script_tools.append(ot);
		}
  }
}

void script_tool_manager_t::fill_menu(tool_selector_t* tool_selector, char const* arg, sint16 /*sound_ok*/) {
  for(uint32 i=0; i<one_click_script_tools.get_count(); i++) {
    tool_exec_script_t* tool = one_click_script_tools[i];
    if(  strcmp(tool->get_menu_arg(), arg)==0  ) {
      tool_selector->add_tool_selector(tool);
    }
  }
	for(uint32 i=0; i<two_click_script_tools.get_count(); i++) {
    tool_exec_two_click_script_t* tool = two_click_script_tools[i];
    if(  strcmp(tool->get_menu_arg(), arg)==0  ) {
      tool_selector->add_tool_selector(tool);
    }
  }
}
