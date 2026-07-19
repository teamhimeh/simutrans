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
#include "components/gui_button.h"
#include "components/gui_container.h"

class roadsign_t;

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

		// per-entry-direction allowed exit checkboxes [entry_idx][exit_idx]
		// entry order: N, S, E, W  (nesw indices 0-3)
		// exit  order: N, E, S, W  (nesw indices 0-3)
		button_t bt_exit[4][4];

		bool has_intersection;
		bool has_choose;
		bool is_multi_way; // true when underlying road/track has 3+ connections

		void rebuild_layout();

	public:
		onewaysign_info_t(roadsign_t* s, koord3d first_intersection);

		const char *get_help_filename() const OVERRIDE {return "onewaysign_info.txt";}

		bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

		void update_data();
};

#endif
