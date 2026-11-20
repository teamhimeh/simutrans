/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_WAY_EDIT_H
#define GUI_WAY_EDIT_H


#include "infra_edit.h"


/*
 * Dialog to select ways, elevated ways, bridges and tunnels of one waytype
 */
class way_edit_frame_t : public infra_edit_frame_t
{
public:
	enum kind_t {
		KIND_GROUND   = 0,
		KIND_ELEVATED = 1,
		KIND_BRIDGE   = 2,
		KIND_TUNNEL   = 3
	};

private:
	button_t bt_ground, bt_elevated, bt_bridge, bt_tunnel;
	gui_numberinput_t ni_min_speed, ni_max_speed, ni_max_cost, ni_max_maintenance;

	bool is_way_waytype_ok(waytype_t wt, bool tram) const;
	bool is_common_filter_ok(const infra_entry_t &e);

	void collect_entries() OVERRIDE;
	void fill_image_choices(const infra_entry_t &e) OVERRIDE;
	void fill_all_image_names() OVERRIDE;
	void append_info(const infra_entry_t &e) OVERRIDE;

public:
	way_edit_frame_t(player_t* player, waytype_t wt);

	const char* get_help_filename() const OVERRIDE { return "way_build.txt"; }

	uint32 get_rdwr_id() OVERRIDE { return magic_way_edit + (waytype & 0xFF); }
};

#endif
