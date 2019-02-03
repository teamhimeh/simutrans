#ifndef coupling_schedule_gui_h
#define coupling_schedule_gui_h

#include "gui_frame.h"

#include "components/gui_label.h"
#include "components/gui_numberinput.h"
#include "components/gui_combobox.h"
#include "components/gui_button.h"
#include "components/action_listener.h"

#include "components/gui_textarea.h"
#include "components/gui_scrollpane.h"

#include "../linehandle_t.h"
#include "../gui/simwin.h"
#include "../gui/schedule_gui.h"
#include "../tpl/vector_tpl.h"


class zeiger_t;
class schedule_t;
struct schedule_entry_t;
class player_t;
class cbuffer_t;
class loadsave_t;

// gui window for selecting coupling and uncoupling point.
class coupling_schedule_gui_t :	public gui_frame_t, public action_listener_t {
	
private:
	schedule_gui_stats_t stats;
	gui_scrollpane_t scrolly;
	player_t* player;
	schedule_t* schedule;
	linehandle_t coupled_line;
	sint16 schedule_index, couple_index, uncouple_index;
	
	gui_combobox_t line_selector;
	gui_label_t lb_line;
	button_t bt_couple, bt_uncouple;
	
	void init_line_selector();
	
public:
	coupling_schedule_gui_t(schedule_t* schedule, sint16 schedule_index, player_t* player);
	// this constructor is only used during loading
	coupling_schedule_gui_t();
	
	void draw(scr_coord pos, scr_size size) OVERRIDE;
	bool infowin_event(event_t const*) OVERRIDE;
	virtual void set_windowsize(scr_size size);
	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif