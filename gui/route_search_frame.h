#ifndef GUI_ROUTE_SEARCH_FRAME_H
#define GUI_ROUTE_SEARCH_FRAME_H

#include "../simhalt.h"
#include "../dataobj/koord.h"
#include "gui_frame.h"
#include "components/action_listener.h"
#include "components/gui_aligned_container.h"
#include "components/gui_button.h"
#include "components/gui_label.h"
#include "components/gui_textinput.h"
#include "components/gui_combobox.h"

/**
 * Route search window. Accepts either halt names or tile coordinates (x,y) as
 * from/to endpoints.  When a coordinate is given it finds all halts serving
 * that tile and passes the coordinate as start_pos so that origin/destination
 * walking cost is factored into the route weight.
 */
class route_search_frame_t : public gui_frame_t, public action_listener_t
{
 private:
	// --- halt-name inputs ---
 	gui_textinput_t from_halt_input, dest_halt_input;
	gui_label_t from_halt_label, dest_halt_label;
	char from_halt_input_text[256];
	char dest_halt_input_text[256];

	// --- coordinate inputs (format "x,y"; empty means use halt name) ---
	gui_textinput_t from_koord_input, dest_koord_input;
	gui_label_t from_koord_label, dest_koord_label;
	char from_koord_text[64];
	char dest_koord_text[64];

	button_t search_button, reverse_search_button;
	gui_aligned_container_t result_container;
	halthandle_t from_halt, dest_halt;
	button_t bt_show_non_traveled;

	vector_tpl<const goods_desc_t *> viewable_freight_types;
	gui_combobox_t freight_type_c;

	uint8 search_ware_index;

	// Parsed from *_koord_text; koord::invalid when the field is empty / invalid.
	koord from_koord, dest_koord;

	void search_route();
	void append_connection_row(haltestelle_t::connection_t connection, halthandle_t from_halt);
	void append_halt_row(halthandle_t halt);
	void swap_halt_inputs();

	// Parse "x,y" text into koord; returns koord::invalid on failure.
	static koord parse_koord(const char* text);

 public:
	route_search_frame_t();
	~route_search_frame_t();

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
