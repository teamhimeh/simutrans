/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_ROADSIGN_EDIT_H
#define GUI_ROADSIGN_EDIT_H


#include "infra_edit.h"


/*
 * Dialog to select signals and road signs of one waytype
 */
class roadsign_edit_frame_t : public infra_edit_frame_t
{
public:
	/// special values of the type filter (others are roadsign_desc_t::types flags)
	enum {
		TYPE_ALL           = -1,
		TYPE_TRAFFIC_LIGHT = -2,
		TYPE_PLAIN_SIGN    = -3
	};

private:
	button_t bt_signal, bt_no_signal;
	gui_combobox_t cb_type;
	gui_numberinput_t ni_min_cost;

	sint32 get_type_filter() const;

	void collect_entries() OVERRIDE;
	void fill_image_choices(const infra_entry_t &e) OVERRIDE;
	void fill_all_image_names() OVERRIDE;
	void append_info(const infra_entry_t &e) OVERRIDE;

public:
	roadsign_edit_frame_t(player_t* player, waytype_t wt);

	const char* get_help_filename() const OVERRIDE { return "roadsign_build.txt"; }

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_roadsign_edit + (waytype & 0xFF); }

	void rdwr( loadsave_t *file ) OVERRIDE;
};

#endif
