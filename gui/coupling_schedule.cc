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

coupling_schedule_gui_t::coupling_schedule_gui_t(schedule_t* schedule, player_t* player):
gui_frame_t( translator::translate("Coupling Schedule"), player),
stats(NULL),
scrolly(&stats),
player(player),
schedule(schedule),
coupled_schedule(NULL),
lb_line("Line:") {
	scr_coord_val ypos = 0;
	
	line_selector.set_pos(scr_coord(2, ypos));
	line_selector.set_size(scr_size(BUTTON4_X-2, D_BUTTON_HEIGHT));
	line_selector.set_max_size(scr_size(BUTTON4_X-2, 13*LINESPACE+D_TITLEBAR_HEIGHT-1));
	line_selector.clear_elements();

	init_line_selector();
	line_selector.add_listener(this);
	add_component(&line_selector);

	ypos += D_BUTTON_HEIGHT+3;
	
	bt_couple.init(button_t::roundbox_state, "Couple", scr_coord(BUTTON1_X, ypos ));
	bt_couple.set_tooltip("Couple at this point");
	bt_couple.add_listener(this);
	bt_couple.pressed = true;
	add_component(&bt_couple);
	bt_decouple.init(button_t::roundbox_state, "Decouple", scr_coord(BUTTON2_X, ypos ));
	bt_decouple.set_tooltip("Couple at this point");
	bt_decouple.add_listener(this);
	bt_decouple.pressed = false;
	add_component(&bt_decouple);
	
	ypos += D_BUTTON_HEIGHT+2;
	
	scrolly.set_pos( scr_coord( 0, ypos ) );
	scrolly.set_show_scroll_x(true);
	scrolly.set_scroll_amount_y(LINESPACE+1);
	add_component(&scrolly);
	
	set_windowsize( scr_size(BUTTON4_X, ypos+D_BUTTON_HEIGHT+(schedule->get_count()>0 ? min(15,schedule->get_count()) : 15)*(LINESPACE+1)+D_TITLEBAR_HEIGHT) );
	set_min_windowsize( scr_size(BUTTON4_X, ypos+D_BUTTON_HEIGHT+3*(LINESPACE+1)+D_TITLEBAR_HEIGHT) );
	set_resizemode(diagonal_resize);
	resize( scr_coord(0,0) );
}

coupling_schedule_gui_t::coupling_schedule_gui_t():
gui_frame_t( translator::translate("Coupling Schedule"), NULL),
stats(NULL),
scrolly(&stats),
lb_line("Line:") {
	
}

void coupling_schedule_gui_t::draw(scr_coord pos, scr_size size) {
	gui_frame_t::draw(pos,size);
}

bool coupling_schedule_gui_t::action_triggered( gui_action_creator_t *komp, value_t p) {
	
	if(  komp==&bt_couple  ) {
		bt_couple.pressed = true;
		bt_decouple.pressed = false;
	}
	else if(  komp==&bt_decouple  ) {
		bt_couple.pressed = false;
		bt_decouple.pressed = true;
	}
	else if(  komp==&line_selector  ) {
		uint32 selection = p.i;
		if(  line_scrollitem_t *li = dynamic_cast<line_scrollitem_t*>(line_selector.get_element(selection))  ) {
			stats.set_schedule(li->get_line()->get_schedule());
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

	player->simlinemgmt.get_lines(schedule->get_type(), &lines);
	
	FOR(  vector_tpl<linehandle_t>,  line,  lines  ) {
		if(  !schedule->matches(welt, line->get_schedule())  ) {
			// do not allow to couple with the same line.
			line_selector.append_element(new line_scrollitem_t(line));
		}
	}

	line_selector.set_selection(-1);
	line_scrollitem_t::sort_mode = line_scrollitem_t::SORT_BY_NAME;
	line_selector.sort( 0, NULL );
}