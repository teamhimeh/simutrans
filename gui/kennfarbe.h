/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_KENNFARBE_H
#define GUI_KENNFARBE_H


#include "../utils/cbuffer_t.h"
#include "gui_frame.h"
#include "components/action_listener.h"
#include "components/gui_textarea.h"
#include "components/gui_button.h"
#include "components/gui_colorbox.h"
#include "components/gui_numberinput.h"
#include "components/gui_textinput.h"
#include "../simline.h"

class choose_color_button_t;

/**
 * Company colors window
 * Dialog to set the player's color
 */
class farbengui_t : public gui_frame_t, action_listener_t
{
	private:
		player_t *player;
		cbuffer_t buf;
		gui_textarea_t txt;

		choose_color_button_t* player_color_1[28];
		choose_color_button_t* player_color_2[28];

		button_t bt_all_line_color_change;

		// Custom color picker widgets
		gui_numberinput_t inp_r1, inp_g1, inp_b1;
		gui_numberinput_t inp_r2, inp_g2, inp_b2;
		gui_colorbox_t    preview_1, preview_2;
		button_t          bt_apply_custom, bt_apply_custom_2;
		char              hex_buf1[8], hex_buf2[8];
		gui_textinput_t   inp_hex1, inp_hex2;
		button_t          bt_os_picker_1, bt_os_picker_2;

		void apply_custom_color();
		void update_preview();

		// -1 = no pick pending, 0 = primary color slot, 1 = secondary color slot
		int pending_os_pick;

	public:
		farbengui_t(player_t *player_);

		/**
		 * Set the window associated helptext
		 * @return the filename for the helptext, or NULL
		 */
		const char * get_help_filename() const OVERRIDE { return "color.txt"; }

		bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

		void draw(scr_coord pos, scr_size size) OVERRIDE;
};

class line_colour_gui_t : public gui_frame_t, action_listener_t
{
	private:
		linehandle_t line;
		player_t *player;

		choose_color_button_t* line_colour[56];

		// Custom color picker widgets for line
		gui_numberinput_t inp_r, inp_g, inp_b;
		gui_colorbox_t    preview;
		button_t          bt_apply_custom;
		char              hex_buf[8];
		gui_textinput_t   inp_hex;
		button_t          bt_os_picker;

		void apply_custom_colour();
		void update_preview();

		bool pending_os_pick;

	public:
		line_colour_gui_t(linehandle_t line_, player_t *player_);

		const char * get_help_filename() const OVERRIDE { return "color.txt"; }

		bool action_triggered(gui_action_creator_t*, value_t ) OVERRIDE;

		void draw(scr_coord pos, scr_size size) OVERRIDE;
};

#endif
