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

/*
 * gui window for selecting coupling and uncoupling point.
 * @author THLeaderH
 * Feb. 2019
 */
 
class coupling_schedule_gui_t :	public gui_frame_t,
						public action_listener_t
{
	gui_combobox_t line_selector;

	schedule_gui_stats_t* stats;
	gui_scrollpane_t scrolly;

	// to add new lines automatically
	uint32 old_line_count;
	uint32 last_schedule_count;
	
	button_t bt_couple, bt_uncouple;
protected:
	schedule_t *schedule, *coupled_line_schedule;
	player_t *player;

	linehandle_t line, coupled_line;
	sint16 couple_index, uncouple_index;

	void init(schedule_t* schedule = NULL, linehandle_t line = linehandle_t(), player_t* player = NULL);

public:
	
	coupling_schedule_gui_t(schedule_t* schedule = NULL, linehandle_t line = linehandle_t(), player_t* player = NULL);

	// for updating info ...
	void init_line_selector();

	bool infowin_event(event_t const*) OVERRIDE;

	const char *get_help_filename() const OVERRIDE {return "coupling_schedule.txt";}

	/**
	 * Draw the Frame
	 */
	void draw(scr_coord pos, scr_size size) OVERRIDE;

	/**
	 * Set window size and adjust component sizes and/or positions accordingly
	 */
	void set_windowsize(scr_size size) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	 /*
	void map_rotate90( sint16 ) OVERRIDE;

	void rdwr( loadsave_t *file ) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_schedule_rdwr_dummy; }
	*/
};

#endif