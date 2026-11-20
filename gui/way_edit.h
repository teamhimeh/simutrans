/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_WAY_EDIT_H
#define GUI_WAY_EDIT_H


#include "infra_edit.h"
#include "components/gui_divider.h"

class tool_build_way_t;
class tool_build_bridge_t;
class tool_build_tunnel_t;


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

	// tool options (same as the dialog when ctrl+clicking the tool in the toolbar)
	gui_aligned_container_t cont_tool_options;
	gui_aligned_container_t cont_modes, cont_road_flags, cont_height_offset, cont_vehicle_offset;
	button_t bt_mode[8];
	button_t bt_avoid_cityroad, bt_citycar_no_entry, bt_no_building, bt_allow_branching_cityroad, bt_pedestrian_no_entry;
	gui_numberinput_t ni_height_offset;
	button_t bt_allow_closed_diagonal;
	gui_numberinput_t ni_vehicle_offset;
	button_t bt_vehicle_offset_mode;

	/// the tools of the selected object (only one of them is set)
	tool_build_way_t *opt_way;
	tool_build_bridge_t *opt_bridge;
	tool_build_tunnel_t *opt_tunnel;
	bool opt_road;

	void init_tool_options();
	void update_tool_options(const infra_entry_t *e) OVERRIDE;
	/// writes overtaking mode and road flags to the tool
	void apply_road_options();

	bool is_way_waytype_ok(waytype_t wt, bool tram) const;
	bool is_common_filter_ok(const infra_entry_t &e);

	void collect_entries() OVERRIDE;
	void fill_image_choices(const infra_entry_t &e) OVERRIDE;
	void fill_all_image_names() OVERRIDE;
	void append_info(const infra_entry_t &e) OVERRIDE;

public:
	way_edit_frame_t(player_t* player, waytype_t wt);

	const char* get_help_filename() const OVERRIDE { return "way_build.txt"; }

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_way_edit + (waytype & 0xFF); }
};

#endif
