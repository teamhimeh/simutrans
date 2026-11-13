/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_GROUND_INFO_H
#define GUI_GROUND_INFO_H


#include "gui_frame.h"
#include "components/gui_location_view_t.h"
#include "../utils/cbuffer_t.h"
#include "components/gui_textarea.h"
#include "components/gui_combobox.h"
#include "components/action_listener.h"

class grund_t;
class weg_t;
class way_desc_t;

class grund_info_t : public gui_frame_t, private action_listener_t
{
protected:
	/**
	 * The ground we observe. The ground will delete this object
	 * if self deleted.
	 */
	cbuffer_t buf, empty;

	const grund_t* gr;

	location_view_t view;
	gui_textarea_t textarea, textarea2;
	gui_combobox_t switch_image_choice;
	bool has_switch_image_choice;
	int switch_standard_index;
	int switch_nse1_index;
	int switch_nse2_index;
	const way_desc_t *switch_choice_desc;
	uint8 switch_choice_ribi;
	sint8 switch_choice_value; // last override value shown in the list (-2: not synced)
	cbuffer_t switch_labels[3]; // item labels must outlive the list items

	void recalc_size();
	void update_switch_image_choice(weg_t *track);
	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

public:
	grund_info_t(const grund_t* gr);

	koord3d get_weltpos(bool) OVERRIDE;

	bool is_weltpos() OVERRIDE;

	void draw(scr_coord pos, scr_size size) OVERRIDE;

	void map_rotate90( sint16 ) OVERRIDE;
};

#endif
