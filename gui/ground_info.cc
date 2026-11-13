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
	has_switch_image_choice(false),
	switch_standard_index(-1),
	switch_nse1_index(-1),
	switch_nse2_index(-1),
	switch_choice_desc(NULL),
	switch_choice_ribi(0)
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
	weg_t *track = gr->get_weg(track_wt);
	if(  track && track->get_desc() && track->get_desc()->has_switch_image() && track->is_switch_junction() && track->is_deletable(welt->get_active_player()) == NULL  ) {
		has_switch_image_choice = true;
		switch_image_choice.set_unsorted();
		update_switch_image_choice(track);
		switch_image_choice.add_listener(this);
		add_component(&switch_image_choice);
	}
	recalc_size();
}

void grund_info_t::update_switch_image_choice(weg_t *track)
{
	if(  !track || !track->get_desc()  ) {
		switch_image_choice.set_visible(false);
		return;
	}
	const uint8 junction_ribi = track->get_ribi_unmasked();
	if(  switch_choice_desc != track->get_desc()  ||  switch_choice_ribi != junction_ribi  ) {
		// label the choices with the actual junction directions (e.g. NSE, NSEW)
		char dirs[8];
		int n = 0;
		if(  junction_ribi & ribi_t::north  ) dirs[n++] = 'N';
		if(  junction_ribi & ribi_t::south  ) dirs[n++] = 'S';
		if(  junction_ribi & ribi_t::east  ) dirs[n++] = 'E';
		if(  junction_ribi & ribi_t::west  ) dirs[n++] = 'W';
		dirs[n] = 0;
		switch_image_choice.clear_elements();
		switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Auto"), SYSCOL_TEXT);
		switch_standard_index = switch_image_choice.count_elements();
		switch_labels[0].clear();
		switch_labels[0].append(dirs);
		switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(switch_labels[0].get_str(), SYSCOL_TEXT);
		switch_nse1_index = switch_image_choice.count_elements();
		switch_labels[1].clear();
		switch_labels[1].append(dirs);
		switch_labels[1].append("1");
		switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(switch_labels[1].get_str(), SYSCOL_TEXT);
		switch_nse2_index = switch_image_choice.count_elements();
		switch_labels[2].clear();
		switch_labels[2].append(dirs);
		switch_labels[2].append("2");
		switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(switch_labels[2].get_str(), SYSCOL_TEXT);
		switch_choice_desc = track->get_desc();
		switch_choice_ribi = junction_ribi;
	}
	const sint8 value = track->get_switch_image_override();
	const int selection = value < 0 ? 0 : (value == 0 ? switch_standard_index : value == 1 ? switch_nse1_index : switch_nse2_index);
	if(  selection >= 0  ) {
		switch_image_choice.set_selection(selection);
	}
	switch_image_choice.set_visible(track->get_desc()->has_switch_image());
}

bool grund_info_t::action_triggered(gui_action_creator_t* comp, value_t)
{
	if(  comp == &switch_image_choice  ) {
		char param[8];
		const int selection = switch_image_choice.get_selection();
		const int value = selection == 0 ? -1 : selection == switch_standard_index ? 0 : selection == switch_nse1_index ? 1 : 2;
		sprintf(param, "%d", value);
		tool_t *tool = tool_t::general_tool[TOOL_SET_SWITCH_IMAGE];
		tool->set_default_param(param);
		bool suspended = false;
		world()->call_work(tool, welt->get_active_player(), gr->get_pos(), suspended);
		return true;
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
	if(  has_switch_image_choice  ) {
		weg_t *track = gr->get_weg(track_wt);
		if(  track  ) {
			update_switch_image_choice(track);
		}
		else switch_image_choice.set_visible(false);
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
