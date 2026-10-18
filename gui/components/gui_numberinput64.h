/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_COMPONENTS_GUI_NUMBERINPUT64_H
#define GUI_COMPONENTS_GUI_NUMBERINPUT64_H

#include "gui_numberinput.h"

// Nonnegative 64-bit input with a fixed positive increment. Listeners read
// get_value(): value_t::i is only 32 bits on Windows.
class gui_numberinput64_t : public gui_action_creator_t, public gui_component_t, public action_listener_t
{
	gui_textinput_t textinp;
	button_t bt_left, bt_right;
	char textbuffer[21];
	sint64 value, min_value, max_value, step;
	bool b_enabled;

	sint64 get_text_value() const;
	sint64 next_value(bool increase) const;
	void notify_change();

public:
	gui_numberinput64_t();
	void set_size(scr_size size) OVERRIDE;
	scr_size get_min_size() const OVERRIDE;
	scr_size get_max_size() const OVERRIDE;
	void set_limits(sint64 minimum, sint64 maximum);
	void set_increment_mode(sint64 increment) { step = increment > 0 ? increment : 1; }
	void set_value(sint64 new_value);
	sint64 get_value() const;
	void enable();
	void disable();
	bool is_focusable() OVERRIDE { return b_enabled && gui_component_t::is_focusable(); }
	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
	bool infowin_event(const event_t*) OVERRIDE;
	void draw(scr_coord offset) OVERRIDE;
	const char *get_accessibility_role() const OVERRIDE { return "numberinput"; }
	const char *get_accessibility_text() const OVERRIDE { return textbuffer; }
};

#endif
