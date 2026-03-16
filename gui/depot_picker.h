/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_DEPOT_PICKER_H
#define GUI_DEPOT_PICKER_H

#include "gui_frame.h"
#include "simwin.h"
#include "components/gui_scrollpane.h"
#include "components/gui_scrolled_list.h"
#include "components/gui_label.h"
#include "components/gui_button.h"
#include "components/gui_image.h"
#include "../convoihandle_t.h"

class depot_t;


/**
 * One row in the depot picker list.
 * Left-clicking selects that depot for the convoy.
 */
class depot_picker_item_t : public gui_aligned_container_t, public gui_scrolled_list_t::scrollitem_t
{
	depot_t        *depot;
	convoihandle_t  cnv;
	bool            teleport;   // true = 'Y' (immediate), false = 'D' (route)
	gui_image_t     waytype_symbol;
	gui_label_buf_t label;
	button_t        gotopos;

	void update_label();
public:
	depot_picker_item_t(depot_t *depot, convoihandle_t cnv, bool teleport);

	void draw(scr_coord pos) OVERRIDE;
	bool infowin_event(const event_t *) OVERRIDE;
	bool is_valid() const OVERRIDE;
	char const* get_text() const OVERRIDE { return ""; }

	static bool compare(const gui_component_t *a, const gui_component_t *b);
};


/**
 * Modal-ish dialog: shows depots of the same waytype/owner as the convoy.
 * Clicking a depot sends (or teleports) the convoy there and closes this window.
 */
class depot_picker_t : public gui_frame_t
{
	gui_scrolled_list_t scrolly;
	convoihandle_t cnv;
	bool teleport;

	void fill_list();
public:
	depot_picker_t(convoihandle_t cnv, bool teleport);

	bool has_min_sizer() const OVERRIDE { return true; }
};

#endif
