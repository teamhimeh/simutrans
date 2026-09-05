/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef onewaysign_info_t_h
#define onewaysign_info_t_h

#include "../simconst.h"
#include "obj_info.h"
#include "components/action_listener.h"
#include "components/gui_action_creator.h"
#include "components/gui_button.h"
#include "components/gui_container.h"
#include "components/gui_label.h"

class roadsign_t;

/**
 * Small clickable diagram of a junction that visualises, and lets the player
 * toggle, the allowed turns of a detailed one-way sign.
 *
 * Rows (entry) follow the same order the info window uses: "from S", "from W",
 * "from N", "from E". Columns (exit) follow ribi_t::nesw: N, E, S, W.
 * Clicking an arrow fires call_listeners() with value == row*4 + col.
 */
class gui_oneway_diagram_t : public gui_component_t, public gui_action_creator_t
{
	private:
		roadsign_t* sign;
		bool active; // true when clicks are meaningful (multi-way + detailed oneway on)

		// local (offset-less) coordinates of the arrow for entry-row/exit-col
		void get_arrow(int row, int col, scr_coord &a, scr_coord &b) const;
		static bool is_uturn(int row, int col);

	public:
		gui_oneway_diagram_t() : sign(NULL), active(false) {}

		void init(roadsign_t* s) { sign = s; }
		void set_active(bool yesno) { active = yesno; }

		scr_size get_min_size() const OVERRIDE;
		scr_size get_max_size() const OVERRIDE { return get_min_size(); }

		void draw(scr_coord offset) OVERRIDE;
		bool infowin_event(const event_t*) OVERRIDE;
};


/**
 * Info window for one-way road/track signs.
 */
class onewaysign_info_t : public obj_infowin_t, public action_listener_t
{
	private:
		roadsign_t* sign;

		// lane-affinity buttons (road only, when intersection found)
		button_t direction[2];

		// length-based choose button
		button_t bt_length_based;

		// detailed_oneway toggle
		button_t bt_detailed_oneway;

		// clickable junction diagram
		gui_oneway_diagram_t diagram;

		gui_label_t lb_diagram;

		// copy / paste the detailed-oneway turn matrix between signs
		button_t bt_copy;
		button_t bt_paste;

		// per-entry-direction allowed exit checkboxes [entry_idx][exit_idx]
		// entry order: N, S, E, W  (nesw indices 0-3)
		// exit  order: N, E, S, W  (nesw indices 0-3)
		button_t bt_exit[4][4];

		bool has_intersection;
		bool has_choose;
		bool is_multi_way; // true when underlying road/track has 3+ connections

		// shared clipboard for copy/paste (packed like the tool 'n'/'e' params)
		static uint8 clip_ns;
		static uint8 clip_ow;
		static bool  clip_valid;

		void rebuild_layout();

		// issue the change tool for toggling one entry->exit turn bit
		void apply_exit_toggle(int row, int col);

	public:
		onewaysign_info_t(roadsign_t* s, koord3d first_intersection);

		const char *get_help_filename() const OVERRIDE {return "onewaysign_info.txt";}

		bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

		void update_data();
};

#endif
