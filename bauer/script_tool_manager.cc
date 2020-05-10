#include "script_tool_manager.h"
#include "../simtool.h"
#include "../sys/simsys.h"
#include "../simdebug.h"
#include "../gui/tool_selector.h"
#include "../utils/searchfolder.h"
#include "../dataobj/tabfile.h"
#include "../simskin.h"
#include "../descriptor/reader/obj_reader.h"

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

tool_t* script_tool_manager_t::load_tool(char const* fullpath, char const* name) {
	if(  !check_file(fullpath)  ) {
		return NULL;
	}
	// open description.tab and get more info
	char buf[PATH_MAX];
	sprintf( buf, "%s/description.tab", fullpath );
	tabfile_t file;
	if (  !file.open(buf)  ) {
		// no description.tab -> handle as one_click script tool
		tool_exec_script_t* tool = new tool_exec_script_t();
		tool->set_default_param(fullpath);
		tool->set_title(name);
		one_click_script_tools.append(tool);
		return tool;
	}
	
	tabfileobj_t contents;
	file.read( contents );
	exec_script_base_t* et; // tool as exec_script_base_t
	tool_t* tool; // tool as tool_t
	// determine one_click tool or two_lick tool.
	if(  strcmp(contents.get_string("type", "one_click"), "two_click")==0  ) {
		// two click tool
		tool_exec_two_click_script_t* tt = new tool_exec_two_click_script_t();
		tool = tt;
		et = tt;
	} else {
		// one click tool
		tool_exec_script_t* ot = new tool_exec_script_t();
		tool = ot;
		et = ot;
	}
	
	tool->set_default_param(fullpath);
	et->set_title(contents.get_string("title", tool->get_default_param()));
	et->set_menu_arg(contents.get_string("menu", tool->get_default_param()));
	if(  contents.get_int("restart", 1) > 0  ) {
		et->enable_restart();
	}
	const char* cursor_name = contents.get_string("icon", "-");
	const skin_desc_t * desc = skinverwaltung_t::get_extra(cursor_name, strlen(cursor_name), skinverwaltung_t::cursor);
	if(  desc  ) {
		tool->cursor = desc->get_image_id(0);
		tool->set_icon(desc->get_image_id(1));
	}
	return tool;
}

void script_tool_manager_t::load_scripts(char const* path) {
  dbg->message("script_tool_manager_t::load_scripts", "Loading scripts from %s", path);
  searchfolder_t find;
  find.search(path, "", true, false);
  FOR(searchfolder_t, const &name, find) {
    char* fullname = new char [strlen(path)+strlen(name)+1];
    sprintf(fullname,"%s%s",path,name);
		
		tool_t* tl = load_tool(fullname, name);
		if(  tool_exec_two_click_script_t* tt = dynamic_cast<tool_exec_two_click_script_t*>(tl)  ) {
			two_click_script_tools.append(tt);
		}
		else if(  tool_exec_script_t* ot = dynamic_cast<tool_exec_script_t*>(tl)  ) {
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
