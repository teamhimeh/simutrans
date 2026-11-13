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
	has_switch_image_choice(false)
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
	if(  track && track->get_desc() && (track->get_desc()->has_switch_image() || track->get_desc()->has_switch_ex_image()) && ribi_t::is_threeway(track->get_ribi_unmasked()) && track->is_deletable(welt->get_active_player()) == NULL  ) {
		has_switch_image_choice = true;
		switch_image_choice.set_unsorted();
		switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Automatic"), SYSCOL_TEXT);
		if(  track->get_desc()->has_switch_image()  ) {
			switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Switch image: NW"), SYSCOL_TEXT);
			switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Switch image: SE"), SYSCOL_TEXT);
		}
		if(  track->get_desc()->has_switch_ex_image()  ) {
			switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Extended switch image: NW"), SYSCOL_TEXT);
			switch_image_choice.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Extended switch image: SE"), SYSCOL_TEXT);
		}
		const sint8 override_value = track->get_switch_image_override();
		const int normal_count = track->get_desc()->has_switch_image() ? 2 : 0;
		switch_image_choice.set_selection(override_value < 0 ? 0 : ((override_value & 2) ? 1 + normal_count + (override_value & 1) : 1 + (override_value & 1)));
		switch_image_choice.add_listener(this);
		add_component(&switch_image_choice);
	}
	recalc_size();
}

bool grund_info_t::action_triggered(gui_action_creator_t* comp, value_t)
{
	if(  comp == &switch_image_choice  ) {
		char param[8];
		const int selection = switch_image_choice.get_selection();
		weg_t *track = gr->get_weg(track_wt);
		const int normal_count = track && track->get_desc() && track->get_desc()->has_switch_image() ? 2 : 0;
		const int value = selection == 0 ? -1 : (selection <= normal_count ? selection - 1 : (selection - normal_count - 1) | 2);
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
			const sint8 value = track->get_switch_image_override();
			const int normal_count = track->get_desc() && track->get_desc()->has_switch_image() ? 2 : 0;
			const int selection = value < 0 ? 0 : ((value & 2) ? 1 + normal_count + (value & 1) : 1 + (value & 1));
			if(  switch_image_choice.get_selection() != selection  ) switch_image_choice.set_selection(selection);
			switch_image_choice.set_visible(ribi_t::is_threeway(track->get_ribi_unmasked()));
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
