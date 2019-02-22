#include "coupling_schedule.h"

#include "../simline.h"
#include "../simhalt.h"
#include "../simworld.h"
#include "../simmenu.h"
#include "../simconvoi.h"
#include "../display/simgraph.h"
#include "../display/viewport.h"

#include "../utils/simstring.h"
#include "../utils/cbuffer_t.h"

#include "../boden/grund.h"

#include "../obj/zeiger.h"

#include "../dataobj/schedule.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/environment.h"

#include "../player/simplay.h"

#include "depot_frame.h"
#include "line_item.h"

#include "karte.h"

coupling_schedule_gui_t::coupling_schedule_gui_t(schedule_t* schedule, linehandle_t line, player_t* player):
gui_frame_t( translator::translate("Coupling Schedule"), player),
stats(new schedule_gui_stats_t()),
scrolly(stats)
{
	init(schedule, line, player);
}


void coupling_schedule_gui_t::init(schedule_t* schedule_, linehandle_t line_, player_t* player_)
{
	player = player_;
	schedule = schedule_;
	line = line_;
	couple_index = uncouple_index = -1;
	stats->add_listener(this);
	// prepare same type but empty schedule for stats.
	coupled_line_schedule = schedule->copy();
	coupled_line_schedule->entries.clear();
	stats->schedule = coupled_line_schedule;
	stats->player = player_;
	
	set_table_layout(1,0);
	new_component<gui_label_t>("Coupling Line:");
	
	line_selector.clear_elements();
	init_line_selector();
	line_selector.add_listener(this);
	add_component(&line_selector);
	
	// action button row
	add_table(2,1)->set_force_equal_columns(true);
	bt_couple.init(button_t::roundbox_state | button_t::flexible, "Couple");
	bt_couple.set_tooltip("Couple at this point");
	bt_couple.add_listener(this);
	bt_couple.pressed = true;
	add_component(&bt_couple);
	bt_uncouple.init(button_t::roundbox_state | button_t::flexible, "Uncouple");
	bt_uncouple.set_tooltip("Uncouple at this point");
	bt_uncouple.add_listener(this);
	bt_uncouple.pressed = false;
	add_component(&bt_uncouple);
	end_table();
	
	scrolly.set_show_scroll_x(true);
	scrolly.set_scroll_amount_y(LINESPACE+1);
	add_component(&scrolly);
	
	set_resizemode(diagonal_resize);

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}

void coupling_schedule_gui_t::draw(scr_coord pos, scr_size size)
{
	
	if(  player->simlinemgmt.get_line_count()!=old_line_count  ||  last_schedule_count!=schedule->get_count()  ) {
		// lines added or deleted
		init_line_selector();
		last_schedule_count = schedule->get_count();
	}
	gui_frame_t::draw(pos,size);
}

bool coupling_schedule_gui_t::action_triggered( gui_action_creator_t *komp, value_t p) {
	
	if(  komp==&bt_couple  ) {
		bt_couple.pressed = true;
		bt_uncouple.pressed = false;
		coupled_line->get_schedule()->set_current_stop(couple_index);
	}
	else if(  komp==&bt_uncouple  ) {
		bt_couple.pressed = false;
		bt_uncouple.pressed = true;
		coupled_line->get_schedule()->set_current_stop(uncouple_index);
	}
	else if(  komp==&line_selector  ) {
		uint32 selection = p.i;
		if(  line_scrollitem_t *li = dynamic_cast<line_scrollitem_t*>(line_selector.get_element(selection))  ) {
			if(  coupled_line.is_bound()  &&  coupled_line!=li->get_line()  ) {
				// different line is selected.
				stats->highlight_schedule( false );
				couple_index = uncouple_index = -1;
				coupled_line->get_schedule()->set_current_stop(couple_index);
			}
			coupled_line = li->get_line();
			if(  coupled_line.is_bound()  ) {
				coupled_line_schedule->copy_from(coupled_line->get_schedule());
				stats->update_schedule();
			}
		}
	}
	else if(  komp==stats  ) {
		// click on one of the schedule entries
		const sint16 idx = p.i;
		if(  idx >= 0 && idx < coupled_line_schedule->get_count()  ) {
			coupled_line_schedule->set_current_stop(idx);
			if(  bt_couple.pressed  ) {
				couple_index = idx;
			} else if(  bt_uncouple.pressed  ){
				uncouple_index = idx;
			}
		}
	}
	return true;
}

void coupling_schedule_gui_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);

	size = get_windowsize()-scr_size(0,D_TITLEBAR_HEIGHT);
	scrolly.set_size(size-scr_size(0,scrolly.get_pos().y));

	//line_selector.set_max_size(scr_size(BUTTON4_X-2, size.h-line_selector.get_pos().y-D_TITLEBAR_HEIGHT));
}

void coupling_schedule_gui_t::init_line_selector()
{
	line_selector.clear_elements();
	vector_tpl<linehandle_t> lines;
	couple_index = uncouple_index = -1;
	old_line_count = player->simlinemgmt.get_line_count();
	last_schedule_count = schedule->get_count();

	player->simlinemgmt.get_lines(schedule->get_type(), &lines);
	
	FOR(  vector_tpl<linehandle_t>,  line,  lines  ) {
		if(  !schedule->matches(welt, line->get_schedule())  ) {
			// do not allow to couple with the same line.
			line_selector.new_component<line_scrollitem_t>(line) ;
		}
	}
	
	line_selector.set_selection(-1);
	line_scrollitem_t::sort_mode = line_scrollitem_t::SORT_BY_NAME;
	line_selector.sort( 0 );
	old_line_count = player->simlinemgmt.get_line_count();
	last_schedule_count = schedule->get_count();
}

/**
 * Mouse clicks are hereby reported to its GUI-Components
 */
bool coupling_schedule_gui_t::infowin_event(const event_t *ev)
{
	if( (ev)->ev_class == EVENT_CLICK  &&  !((ev)->ev_code==MOUSE_WHEELUP  ||  (ev)->ev_code==MOUSE_WHEELDOWN)  &&  !line_selector.getroffen(ev->cx, ev->cy-D_TITLEBAR_HEIGHT)  )  {

		// close combo box; we must do it ourselves, since the box does not receive outside events ...
		line_selector.close_box();
	}
	else if(  ev->ev_class == INFOWIN  &&  ev->ev_code == WIN_CLOSE  &&  schedule!=NULL  ) {
		stats->highlight_schedule(false);
		// now apply the changes
		if(  couple_index!=-1  &&  uncouple_index!=-1  ) {
			schedule->append_coupling(line, coupled_line, couple_index, uncouple_index);
		}
	}

	return gui_frame_t::infowin_event(ev);
}