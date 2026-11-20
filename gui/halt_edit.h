/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_HALT_EDIT_H
#define GUI_HALT_EDIT_H


#include "infra_edit.h"


/*
 * Dialog to select stops, extension buildings and harbours of one waytype
 */
class halt_edit_frame_t : public infra_edit_frame_t
{
public:
	enum kind_t {
		KIND_STOP      = 0,
		KIND_EXTENSION = 1,
		KIND_HARBOUR   = 2
	};

private:
	button_t bt_stop, bt_extension, bt_harbour;
	button_t bt_pax, bt_mail, bt_ware;
	button_t bt_dims_end, bt_dims_many, bt_dims_diagonal, bt_dims_slope;
	gui_numberinput_t ni_min_level, ni_max_level, ni_max_cost;

	/// layout filters are only meaningful for stops
	void update_layout_filters();

	void collect_entries() OVERRIDE;
	void fill_image_choices(const infra_entry_t &e) OVERRIDE;
	void fill_all_image_names() OVERRIDE;
	void append_info(const infra_entry_t &e) OVERRIDE;
	void update_preview() OVERRIDE;
	void filter_changed() OVERRIDE;
	void filter_state_loaded() OVERRIDE { update_layout_filters(); }
	void new_thumbnail(const infra_entry_t &e, const char *name) OVERRIDE;

public:
	halt_edit_frame_t(player_t* player, waytype_t wt);

	const char* get_help_filename() const OVERRIDE { return "halt_build.txt"; }

	uint32 get_rdwr_id() OVERRIDE { return magic_halt_edit + (waytype & 0xFF); }
};

#endif
