/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_WAYOBJ_EDIT_H
#define GUI_WAYOBJ_EDIT_H


#include "infra_edit.h"


/*
 * Dialog to select way objects (e.g. overhead lines) of one waytype
 */
class wayobj_edit_frame_t : public infra_edit_frame_t
{
private:
	gui_numberinput_t ni_min_speed, ni_max_speed, ni_max_cost, ni_max_maintenance;

	void collect_entries() OVERRIDE;
	void fill_image_choices(const infra_entry_t &e) OVERRIDE;
	void fill_all_image_names() OVERRIDE;
	void append_info(const infra_entry_t &e) OVERRIDE;

public:
	wayobj_edit_frame_t(player_t* player, waytype_t wt);

	const char* get_help_filename() const OVERRIDE { return "wayobj_build.txt"; }

	uint32 get_rdwr_id() OVERRIDE { return magic_wayobj_edit + (waytype & 0xFF); }
};

#endif
