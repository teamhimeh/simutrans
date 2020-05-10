#include "script_tool_manager.h"
#include "../simtool.h"
#include "../sys/simsys.h"
#include "../simdebug.h"
#include "../gui/tool_selector.h"
#include "../utils/searchfolder.h"
#include "../dataobj/tabfile.h"
#include "../simskin.h"
#include "../descriptor/reader/obj_reader.h"

vector_tpl<tool_exec_script_t*> script_tool_manager_t::script_tools;

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

void script_tool_manager_t::load_scripts(char const* path) {
  dbg->message("script_tool_manager_t::load_scripts", "Loading scripts from %s", path);
  searchfolder_t find;
  find.search(path, "", true, false);
  FOR(searchfolder_t, const &name, find) {
    char* fullname = new char [strlen(path)+strlen(name)+1];
    sprintf(fullname,"%s%s",path,name);
    if(  !check_file(fullname)  ) {
      continue;
    }
    // register script
    tool_exec_script_t* tool = new tool_exec_script_t();
    tool->set_default_param(fullname);
    tool->set_title(name);
    script_tools.append(tool);
    
    // open description.tab and get more info
    char buf[PATH_MAX];
  	sprintf( buf, "%s/description.tab", fullname );
    tabfile_t file;
  	if (  !file.open(buf)  ) {
  		continue;
  	}
    tabfileobj_t contents;
    file.read( contents );
    tool->set_title(contents.get_string("title", tool->get_default_param()));
    tool->set_menu_arg(contents.get_string("menu", tool->get_default_param()));
    const char* cursor_name = contents.get_string("icon", "-");
    const skin_desc_t * desc = skinverwaltung_t::get_extra(cursor_name, strlen(cursor_name), skinverwaltung_t::cursor);
		if(  desc  ) {
			tool->cursor = desc->get_image_id(0);
			tool->set_icon(desc->get_image_id(1));
		}
  }
}

void script_tool_manager_t::fill_menu(tool_selector_t* tool_selector, char const* arg, sint16 /*sound_ok*/) {
  for(uint32 i=0; i<script_tools.get_count(); i++) {
    tool_exec_script_t* tool = script_tools[i];
    if(  strcmp(tool->get_menu_arg(), arg)==0  ) {
      tool_selector->add_tool_selector(tool);
    }
  }
}
