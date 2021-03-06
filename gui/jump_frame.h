/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_JUMP_FRAME_H
#define GUI_JUMP_FRAME_H


#include "components/action_listener.h"
#include "gui_frame.h"
#include "components/gui_textinput.h"
#include "components/gui_button.h"
#include "components/gui_numberinput.h"



class jump_frame_t : public gui_frame_t, action_listener_t
{
	char buf[64];
	gui_textinput_t input;
	button_t jumpbutton, auto_jump_button;
	gui_numberinput_t auto_jump_interval_numberinput;

public:
	static bool auto_jump;
	static time_t auto_jump_base_time;
	static uint16 auto_jump_interval;
	static char auto_jump_countdown_char[6];
	
	jump_frame_t();

	/**
	* Set the window associated helptext
	* @return the filename for the helptext, or NULL
	*/
	const char * get_help_filename() const OVERRIDE { return "jump_frame.txt"; }

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
