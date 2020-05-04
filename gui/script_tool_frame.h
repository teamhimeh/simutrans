/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_SCRIPT_TOOL_INFO_H
#define GUI_SCRIPT_TOOL_INFO_H


#include "savegame_frame.h"
#include "../utils/cbuffer_t.h"


class dynamic_string;
class script_tool_frame_t : public savegame_frame_t
{
private:
	cbuffer_t path;

protected:
	/**
	 * Action that's started by the press of a button.
	 */
	bool item_action(const char *fullpath) OVERRIDE;

	/**
	 * Action, started after X-Button pressing
	 */
	bool del_action(const char *f) OVERRIDE { return item_action(f); }

	// returns extra file info
	const char *get_info(const char *fname) OVERRIDE;

	// true, if valid
	bool check_file( const char *filename, const char *suffix ) OVERRIDE;

public:
	/**
	* Set the window associated helptext
	* @return the filename for the helptext, or NULL
	*/
	const char * get_help_filename() const OVERRIDE { return "script_tool.txt"; }

	script_tool_frame_t();
};

#endif
