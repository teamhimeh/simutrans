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
#include "components/gui_label.h"
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
	/// one choice list per way on the tile that has a fixable switch junction
	enum { MAX_SWITCH_CHOICES = 2 };
	struct switch_choice_t {
		gui_label_t label;
		gui_combobox_t list;
		waytype_t waytype;
		const way_desc_t *desc;   // add-on the list was built for
		uint8 ribi;               // junction shape the list was built for
		sint8 shown_value;        // last override value shown in the list (-2: not synced)
		sint8 values[6];          // override value of each list entry
		uint8 count;
		switch_choice_t() : waytype(invalid_wt), desc(NULL), ribi(0), shown_value(-2), count(0) {}
	};
	switch_choice_t switch_choice[MAX_SWITCH_CHOICES];
	uint8 switch_choice_count;

	void recalc_size();
	void update_switch_image_choice(switch_choice_t &choice);
	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

public:
	grund_info_t(const grund_t* gr);

	koord3d get_weltpos(bool) OVERRIDE;

	bool is_weltpos() OVERRIDE;

	void draw(scr_coord pos, scr_size size) OVERRIDE;

	void map_rotate90( sint16 ) OVERRIDE;
};

#endif
