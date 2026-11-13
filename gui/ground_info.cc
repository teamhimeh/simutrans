/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../display/simgraph.h"
#include "../display/viewport.h"
#include "../simworld.h"
#include "../dataobj/translator.h"
#include "ground_info.h"
#include "../boden/grund.h"
#include "../boden/wege/weg.h"
#include "../simmenu.h"
#include "../simtool.h"



grund_info_t::grund_info_t(const grund_t* gr_) :
	gui_frame_t("", NULL),
	gr(gr_),
	view(gr_->get_pos(), scr_size( max(64, get_base_tile_raster_width()), max(56, (get_base_tile_raster_width()*7)/8) )),
	textarea(&buf),
	textarea2(&buf),
	switch_choice_count(0)
{
	const obj_t *const d = gr->obj_bei(0);
	if (  d!=NULL  ) {
		set_owner( d->get_owner() );
	}
	buf.clear();

	set_table_layout(1, 0);
	set_alignment(ALIGN_CENTER_H);
	add_table(2, 1)->set_alignment(ALIGN_LEFT | ALIGN_TOP);
	{
		add_component(&textarea);
		add_component(&view);
	}
	end_table();
	add_component(&textarea2);
	// every way of a switch junction can have its image fixed
	for(  uint8 i = 0;  i < 2;  i++  ) {
		weg_t *way = gr->get_weg_nr(i);
		if(  way  &&  way->can_fix_switch_image()  &&  way->is_deletable(welt->get_active_player()) == NULL  ) {
			switch_choice_t &choice = switch_choice[switch_choice_count++];
			choice.waytype = way->get_waytype();
			choice.list.set_unsorted();
			choice.list.add_listener(this);
			update_switch_image_choice(choice);
			if(  gr->get_weg_nr(1)  ) {
				// two ways on the tile: tell them apart
				choice.label.set_text_pointer(translator::translate(way->get_desc()->get_name()));
				add_component(&choice.label);
			}
			add_component(&choice.list);
		}
	}
	recalc_size();
}

/// directions the branch of a T junction turns to (index: N, E, S, W)
static const char *const switch_branch_labels[4] = { "Branch north", "Branch east", "Branch south", "Branch west" };
static const char *const switch_y_labels[4] = { "Y-junction (north)", "Y-junction (east)", "Y-junction (south)", "Y-junction (west)" };


/// Label of the fixed choice @p value for a junction with the connections @p ribi.
static const char *switch_choice_label(uint8 ribi, sint8 value)
{
	if(  value < 0  ) {
		return translator::translate("Auto");
	}
	if(  value == 0  ) {
		return translator::translate("Standard");
	}
	if(  ribi == ribi_t::all  ) {
		static const char *const labels[4] = { "Crossover A", "Crossover B", "Double crossover A", "Double crossover B" };
		return translator::translate(labels[value-1]);
	}
	// Side of the main line the branch of the junction curves to, as drawn inside this tile.
	// The first state (nw) is the one the automatic selection picks when the neighbour of the
	// branch curves away; its curve inside the tile joins the branch with the opposite end of
	// the main line (e.g. nse2 is a south-east curve). The second state uses the other end.
	int dir; // index N, E, S, W
	switch(  ribi  ) {
		case ribi_t::northsoutheast: dir = 2; break; // south
		case ribi_t::northeastwest:  dir = 3; break; // west
		case ribi_t::northsouthwest: dir = 0; break; // north
		default:                     dir = 1; break; // south-east-west: east
	}
	if(  value == 2  ||  value == 4  ) {
		dir = (dir+2) & 3;
	}
	return translator::translate( value <= 2 ? switch_branch_labels[dir] : switch_y_labels[dir] );
}


void grund_info_t::update_switch_image_choice(switch_choice_t &choice)
{
	weg_t *way = gr->get_weg(choice.waytype);
	if(  !way  ||  !way->can_fix_switch_image()  ) {
		// the junction was resolved while the dialog is open
		choice.label.set_visible(false);
		choice.list.set_visible(false);
		return;
	}
	const uint8 junction_ribi = way->get_ribi_unmasked();
	if(  choice.desc != way->get_desc()  ||  choice.ribi != junction_ribi  ) {
		// offer only the choices the add-on has images for
		choice.list.clear_elements();
		choice.count = 0;
		for(  sint8 value = -1;  value <= 4;  value++  ) {
			if(  way->is_switch_image_choice_available(value)  ) {
				choice.list.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(switch_choice_label(junction_ribi, value), SYSCOL_TEXT);
				choice.values[choice.count++] = value;
			}
		}
		choice.desc = way->get_desc();
		choice.ribi = junction_ribi;
		choice.shown_value = -2; // force selection sync below
	}
	// Follow the way only when its value changed. Setting the selection every frame
	// would revert the user's pick until a networked command has been executed.
	const sint8 value = way->get_switch_image_override();
	if(  value != choice.shown_value  ) {
		int selection = 0; // Auto
		for(  uint8 i = 0;  i < choice.count;  i++  ) {
			if(  choice.values[i] == value  ) {
				selection = i;
			}
		}
		choice.list.set_selection(selection);
		choice.shown_value = value;
	}
	choice.label.set_visible(true);
	choice.list.set_visible(true);
}

bool grund_info_t::action_triggered(gui_action_creator_t* comp, value_t)
{
	for(  uint8 i = 0;  i < switch_choice_count;  i++  ) {
		switch_choice_t &choice = switch_choice[i];
		if(  comp == &choice.list  ) {
			const int selection = choice.list.get_selection();
			if(  selection < 0  ||  selection >= choice.count  ) {
				return true;
			}
			char param[16];
			sprintf(param, "%d,%d", (int)choice.waytype, (int)choice.values[selection]);
			tool_t *tool = tool_t::general_tool[TOOL_SET_SWITCH_IMAGE];
			tool->set_default_param(param);
			bool suspended = false;
			world()->call_work(tool, welt->get_active_player(), gr->get_pos(), suspended);
			return true;
		}
	}
	return false;
}


void grund_info_t::recalc_size()
{
	textarea.set_buf(&buf);
	scr_size sz = textarea.get_size();
	if (sz.w == 0) {
		textarea.set_visible(false);
		textarea2.set_visible(false);
	}
	else if(sz.h==LINESPACE) {
		textarea.set_visible(false);
		textarea2.set_size(sz);
		textarea2.set_visible(true);
	}
	else {
		textarea.set_visible(true);
		textarea2.set_visible(false);
	}
	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}



/**
 * Draw new component. The values to be passed refer to the window
 * i.e. It's the screen coordinates of the window where the
 * component is displayed.
 */
void grund_info_t::draw(scr_coord pos, scr_size size)
{
	// update for owner and name change
	set_dirty();
	const obj_t *const d = gr->obj_bei(0);
	if (  d!=NULL  ) {
		set_owner( d->get_owner() );
	}
	gui_frame_t::set_name( translator::translate(gr->get_name()) );
	for(  uint8 i = 0;  i < switch_choice_count;  i++  ) {
		update_switch_image_choice(switch_choice[i]);
	}

	const cbuffer_t old_buf(buf);
	buf.clear();
	gr->info(buf);
	if(  strcmp( buf, old_buf )  ) {
		recalc_size();
	}

	gui_frame_t::draw(pos, size);
}


koord3d grund_info_t::get_weltpos(bool)
{
	return gr->get_pos();
}


bool grund_info_t::is_weltpos()
{
	return ( welt->get_viewport()->is_on_center( gr->get_pos() ) );
}


void grund_info_t::map_rotate90( sint16 new_ysize )
{
	view.map_rotate90(new_ysize);
}
