/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "kennfarbe.h"
#include "../simworld.h"
#include "../simskin.h"
#include "../dataobj/environment.h"
#include "../dataobj/translator.h"
#include "../player/simplay.h"
#include "components/gui_label.h"
#include "components/gui_image.h"
#include "../simtool.h"
#include "../simline.h"
#include "../display/simgraph.h"
#include "../sys/simsys.h"
#include "messagebox.h"
#include "simwin.h"
#include <stdio.h>
#include <string.h>

/**
 * Buttons forced to be square ...
 */
class choose_color_button_t : public button_t
{
	scr_coord_val w;
public:
	choose_color_button_t() : button_t()
	{
		w = max(D_BUTTON_HEIGHT, display_get_char_width('X') + D_BUTTON_PADDINGS_X);
	}
	scr_size get_min_size() const OVERRIDE
	{
		return scr_size(w, D_BUTTON_HEIGHT);
	}
};

// Format R,G,B into "#RRGGBB" in buf (must be at least 8 bytes)
static void rgb_to_hex(uint8 r, uint8 g, uint8 b, char *buf)
{
	snprintf(buf, 8, "#%02X%02X%02X", r, g, b);
}

// Parse "#RRGGBB" or "RRGGBB" → r,g,b. Returns true on success.
static bool hex_to_rgb(const char *buf, uint8 &r, uint8 &g, uint8 &b)
{
	const char *s = buf;
	if (*s == '#') s++;
	if (strlen(s) != 6) return false;
	unsigned rv, gv, bv;
	if (sscanf(s, "%2x%2x%2x", &rv, &gv, &bv) != 3) return false;
	r = (uint8)rv;
	g = (uint8)gv;
	b = (uint8)bv;
	return true;
}

// Build the tool param string for a custom RGB color change
static void send_custom_player_color(player_t *player, char which,
                                     uint8 r1, uint8 g1, uint8 b1,
                                     uint8 r2, uint8 g2, uint8 b2)
{
	cbuffer_t buf;
	buf.printf("%c%u,#%02x%02x%02x,#%02x%02x%02x",
	           which, player->get_player_nr(),
	           r1, g1, b1, r2, g2, b2);
	tool_t *w = create_tool(TOOL_RECOLOUR_TOOL | SIMPLE_TOOL);
	w->set_default_param(buf);
	world()->set_tool(w, player);
	delete w;
}


// ---------------------------------------------------------------
// line_colour_gui_t
// ---------------------------------------------------------------

line_colour_gui_t::line_colour_gui_t(linehandle_t line_, player_t *player_) :
	gui_frame_t( translator::translate("Line Colour"), player_ )
{
	line = line_;
	player = player_;
	pending_os_pick = false;

	set_table_layout(1, 0);

	// Line's colour label
	new_component<gui_label_t>("Line Colour:");

	add_table(14,4);

	//Line colour buttons (preset palette)
	for(unsigned i=0;  i<28;  i++) {
		line_colour[i] = new_component<choose_color_button_t>();
		line_colour[i]->init( button_t::box_state, (" "));
		line_colour[i]->background_color = color_idx_to_rgb(i*8+env_t::gui_player_color_bright);
		line_colour[i]->add_listener(this);
	}
	for(unsigned i=0;  i<28;  i++) {
		line_colour[i+28] = new_component<choose_color_button_t>();
		line_colour[i+28]->init( button_t::box_state, (" "));
		line_colour[i+28]->background_color = color_idx_to_rgb(i*8);
		line_colour[i+28]->add_listener(this);
	}
	// Mark currently selected preset if it matches
	if(line.is_bound()) {
		const PIXVAL cur = line->get_colour();
		for(unsigned i = 0; i < 56; i++) {
			if(line_colour[i]->background_color == cur) {
				line_colour[i]->pressed = true;
				break;
			}
		}
	}
	end_table();

	// Custom color picker section
	new_component<gui_label_t>("Custom colour (R G B):");
	add_table(5, 1);
	{
		// Prefill from current line color
		uint8 cr = 128, cg = 128, cb = 128;
		if(line.is_bound()) {
			pixval_to_rgb8(line->get_colour(), cr, cg, cb);
		}
		inp_r.init(cr, 0, 255, 1, false, 3, false);
		inp_g.init(cg, 0, 255, 1, false, 3, false);
		inp_b.init(cb, 0, 255, 1, false, 3, false);
		inp_r.add_listener(this);
		inp_g.add_listener(this);
		inp_b.add_listener(this);
		add_component(&inp_r);
		add_component(&inp_g);
		add_component(&inp_b);

		preview.set_color(make_rgb_pixval(cr, cg, cb));
		preview.set_max_size(scr_size(D_BUTTON_HEIGHT * 2, D_BUTTON_HEIGHT * 1));
		add_component(&preview);

		bt_apply_custom.init(button_t::roundbox, translator::translate("Apply"));
		bt_apply_custom.add_listener(this);
		add_component(&bt_apply_custom);
	}
	end_table();

	// Hex input and OS color picker row
	add_table(2, 1);
	{
		rgb_to_hex((uint8)inp_r.get_value(), (uint8)inp_g.get_value(), (uint8)inp_b.get_value(), hex_buf);
		inp_hex.set_text(hex_buf, sizeof(hex_buf));
		inp_hex.add_listener(this);
		add_component(&inp_hex);

		bt_os_picker.init(button_t::roundbox, translator::translate("Color Picker"));
		bt_os_picker.add_listener(this);
		add_component(&bt_os_picker);
	}
	end_table();

	reset_min_windowsize();
}

void line_colour_gui_t::update_preview()
{
	const uint8 r = (uint8)inp_r.get_value();
	const uint8 g = (uint8)inp_g.get_value();
	const uint8 b = (uint8)inp_b.get_value();
	preview.set_color(make_rgb_pixval(r, g, b));
	rgb_to_hex(r, g, b, hex_buf);
}

void line_colour_gui_t::apply_custom_colour()
{
	if(!line.is_bound()) return;
	const uint8 r = (uint8)inp_r.get_value();
	const uint8 g = (uint8)inp_g.get_value();
	const uint8 b = (uint8)inp_b.get_value();

	// Deselect all preset buttons
	for(unsigned i = 0; i < 56; i++) {
		line_colour[i]->pressed = false;
	}

	cbuffer_t buf;
	buf.printf("o,%i,#%02x%02x%02x", line.get_id(), r, g, b);
	tool_t *w = create_tool(TOOL_CHANGE_LINE | SIMPLE_TOOL);
	w->set_default_param(buf);
	world()->set_tool(w, player);
	delete w;
}

bool line_colour_gui_t::action_triggered(gui_action_creator_t *comp, value_t /* */)
{
	if(comp == &bt_apply_custom) {
		apply_custom_colour();
		return true;
	}
	if(comp == &inp_r || comp == &inp_g || comp == &inp_b) {
		update_preview();
		return true;
	}
	if(comp == &inp_hex) {
		uint8 r, g, b;
		if(hex_to_rgb(hex_buf, r, g, b)) {
			inp_r.set_value(r);
			inp_g.set_value(g);
			inp_b.set_value(b);
			preview.set_color(make_rgb_pixval(r, g, b));
		}
		return true;
	}
	if(comp == &bt_os_picker) {
		uint8 r = (uint8)inp_r.get_value();
		uint8 g = (uint8)inp_g.get_value();
		uint8 b = (uint8)inp_b.get_value();
		pending_os_pick = dr_pick_color_start(r, g, b);
		if(!pending_os_pick) {
			create_win( new news_img("A color picker is already open."), w_time_delete, magic_none );
		}
		return true;
	}

	for(unsigned i=0;  i<56;  i++){
		if(comp==line_colour[i]) {
			for(unsigned j=0;  j<56;  j++) {
				line_colour[j]->pressed = false;
			}
			line_colour[i]->pressed = true;

			if (line.is_bound()) {
				const PIXVAL col = line_colour[i]->background_color;
				// Update custom inputs to match selected preset
				uint8 r, g, b;
				pixval_to_rgb8(col, r, g, b);
				inp_r.set_value(r);
				inp_g.set_value(g);
				inp_b.set_value(b);
				preview.set_color(col);

				cbuffer_t buf;
				buf.printf("o,%i,#%02x%02x%02x", line.get_id(), r, g, b);
				tool_t *w = create_tool(TOOL_CHANGE_LINE | SIMPLE_TOOL);
				w->set_default_param(buf);
				world()->set_tool(w, player);
				delete w;
				return true;
			}
		}
	}

	return false;
}


void line_colour_gui_t::draw(scr_coord pos, scr_size size)
{
	if(pending_os_pick) {
		uint8 r, g, b;
		const color_pick_result_t res = dr_pick_color_poll(r, g, b);
		if(res == COLOR_PICK_OK) {
			inp_r.set_value(r);
			inp_g.set_value(g);
			inp_b.set_value(b);
			preview.set_color(make_rgb_pixval(r, g, b));
			rgb_to_hex(r, g, b, hex_buf);
		}
		if(res != COLOR_PICK_RUNNING) {
			pending_os_pick = false;
		}
	}
	gui_frame_t::draw(pos, size);
}


// ---------------------------------------------------------------
// farbengui_t
// ---------------------------------------------------------------

farbengui_t::farbengui_t(player_t *player_) :
	gui_frame_t( translator::translate("Farbe"), player_ ),
	txt(&buf)
{
	player = player_;
	pending_os_pick = -1;
	buf.clear();
	buf.append(translator::translate("COLOR_CHOOSE\n"));

	set_table_layout(1,0);

	add_table(2,1);
	txt.recalc_size();
	add_component( &txt );
	new_component<gui_image_t>(skinverwaltung_t::color_options->get_image_id(0), player->get_player_nr(), ALIGN_NONE, true);
	end_table();

	// Get all colors used by other players
	uint32 used_colors1 = 0;
	uint32 used_colors2 = 0;
	for(  int i=0;  i<MAX_PLAYER_COUNT;  i++  ) {
		if(  i!=player->get_player_nr()  &&  welt->get_player(i)  ) {
			used_colors1 |= 1 << (welt->get_player(i)->get_player_color1() / 8);
			used_colors2 |= 1 << (welt->get_player(i)->get_player_color2() / 8);
		}
	}

	// Primary color preset buttons
	new_component<gui_label_t>("Your primary color:");
	add_table(14,2);
	for(unsigned i=0;  i<28;  i++) {
		player_color_1[i] = new_component<choose_color_button_t>();
		player_color_1[i]->init( button_t::box_state, (used_colors1 & (1<<i) ? "X" : " "));
		player_color_1[i]->background_color = color_idx_to_rgb(i*8+env_t::gui_player_color_bright);
		player_color_1[i]->add_listener(this);
	}
	if(!player->is_player_color_custom()) {
		player_color_1[player->get_player_color1()/8]->pressed = true;
	}
	end_table();

	// Primary custom color picker
	new_component<gui_label_t>("Custom primary (R G B):");
	add_table(5, 1);
	{
		uint8 r = 128, g = 128, b = 128;
		if(player->is_player_color_custom()) {
			r = player->get_player_color_r(0);
			g = player->get_player_color_g(0);
			b = player->get_player_color_b(0);
		}
		else {
			pixval_to_rgb8(player->get_player_color1_pixval(env_t::gui_player_color_bright), r, g, b);
		}
		inp_r1.init(r, 0, 255, 1, false, 3, false);
		inp_g1.init(g, 0, 255, 1, false, 3, false);
		inp_b1.init(b, 0, 255, 1, false, 3, false);
		inp_r1.add_listener(this); inp_g1.add_listener(this); inp_b1.add_listener(this);
		add_component(&inp_r1); add_component(&inp_g1); add_component(&inp_b1);
		preview_1.set_color(make_rgb_pixval(r, g, b));
		preview_1.set_max_size(scr_size(D_BUTTON_HEIGHT * 2, D_BUTTON_HEIGHT * 1));
		add_component(&preview_1);
		bt_apply_custom.init(button_t::roundbox, translator::translate("Apply"));
		bt_apply_custom.add_listener(this);
		add_component(&bt_apply_custom);
	}
	end_table();

	// Primary hex + OS picker row
	add_table(2, 1);
	{
		uint8 r = (uint8)inp_r1.get_value(), g = (uint8)inp_g1.get_value(), b = (uint8)inp_b1.get_value();
		rgb_to_hex(r, g, b, hex_buf1);
		inp_hex1.set_text(hex_buf1, sizeof(hex_buf1));
		inp_hex1.add_listener(this);
		add_component(&inp_hex1);

		bt_os_picker_1.init(button_t::roundbox, translator::translate("Color Picker"));
		bt_os_picker_1.add_listener(this);
		add_component(&bt_os_picker_1);
	}
	end_table();

	// Secondary color preset buttons
	new_component<gui_label_t>("Your secondary color:");
	add_table(14,2);
	for(unsigned i=0;  i<28;  i++) {
		player_color_2[i] = new_component<choose_color_button_t>();
		player_color_2[i]->init( button_t::box_state, (used_colors2 & (1<<i) ? "X" : " "));
		player_color_2[i]->background_color = color_idx_to_rgb(i*8+env_t::gui_player_color_bright);
		player_color_2[i]->add_listener(this);
	}
	if(!player->is_player_color_custom()) {
		player_color_2[player->get_player_color2()/8]->pressed = true;
	}
	end_table();

	// Secondary custom color picker
	new_component<gui_label_t>("Custom secondary (R G B):");
	add_table(5, 1);
	{
		uint8 r = 128, g = 128, b = 128;
		if(player->is_player_color_custom()) {
			r = player->get_player_color_r(1);
			g = player->get_player_color_g(1);
			b = player->get_player_color_b(1);
		}
		else {
			pixval_to_rgb8(player->get_player_color2_pixval(env_t::gui_player_color_bright), r, g, b);
		}
		inp_r2.init(r, 0, 255, 1, false, 3, false);
		inp_g2.init(g, 0, 255, 1, false, 3, false);
		inp_b2.init(b, 0, 255, 1, false, 3, false);
		inp_r2.add_listener(this); inp_g2.add_listener(this); inp_b2.add_listener(this);
		add_component(&inp_r2); add_component(&inp_g2); add_component(&inp_b2);
		preview_2.set_color(make_rgb_pixval(r, g, b));
		preview_2.set_max_size(scr_size(D_BUTTON_HEIGHT * 2, D_BUTTON_HEIGHT * 1));
		add_component(&preview_2);
		bt_apply_custom_2.init(button_t::roundbox, translator::translate("Apply"));
		bt_apply_custom_2.add_listener(this);
		add_component(&bt_apply_custom_2);
	}
	end_table();

	// Secondary hex + OS picker row
	add_table(2, 1);
	{
		uint8 r2 = (uint8)inp_r2.get_value(), g2 = (uint8)inp_g2.get_value(), b2 = (uint8)inp_b2.get_value();
		rgb_to_hex(r2, g2, b2, hex_buf2);
		inp_hex2.set_text(hex_buf2, sizeof(hex_buf2));
		inp_hex2.add_listener(this);
		add_component(&inp_hex2);

		bt_os_picker_2.init(button_t::roundbox, translator::translate("Color Picker"));
		bt_os_picker_2.add_listener(this);
		add_component(&bt_os_picker_2);
	}
	end_table();

	bt_all_line_color_change.init(button_t::roundbox, "Change line color as current player color");
	bt_all_line_color_change.add_listener(this);
	add_component(&bt_all_line_color_change);

	reset_min_windowsize();
}

void farbengui_t::update_preview()
{
	const uint8 r1 = (uint8)inp_r1.get_value(), g1 = (uint8)inp_g1.get_value(), b1 = (uint8)inp_b1.get_value();
	const uint8 r2 = (uint8)inp_r2.get_value(), g2 = (uint8)inp_g2.get_value(), b2 = (uint8)inp_b2.get_value();
	preview_1.set_color(make_rgb_pixval(r1, g1, b1));
	preview_2.set_color(make_rgb_pixval(r2, g2, b2));
	rgb_to_hex(r1, g1, b1, hex_buf1);
	rgb_to_hex(r2, g2, b2, hex_buf2);
}

void farbengui_t::apply_custom_color()
{
	// Deselect all preset buttons
	for(unsigned i = 0; i < 28; i++) {
		player_color_1[i]->pressed = false;
		player_color_2[i]->pressed = false;
	}
	const uint8 r1 = (uint8)inp_r1.get_value(), g1 = (uint8)inp_g1.get_value(), b1 = (uint8)inp_b1.get_value();
	const uint8 r2 = (uint8)inp_r2.get_value(), g2 = (uint8)inp_g2.get_value(), b2 = (uint8)inp_b2.get_value();
	send_custom_player_color(player, '1', r1, g1, b1, r2, g2, b2);
}

bool farbengui_t::action_triggered( gui_action_creator_t *comp, value_t /* */)
{
	// Custom color apply button
	if(comp == &bt_apply_custom || comp == &bt_apply_custom_2) {
		apply_custom_color();
		return true;
	}
	// Preview update on RGB number input change
	if(comp == &inp_r1 || comp == &inp_g1 || comp == &inp_b1 ||
	   comp == &inp_r2 || comp == &inp_g2 || comp == &inp_b2) {
		update_preview();
		return true;
	}
	// Hex input → RGB sync
	if(comp == &inp_hex1) {
		uint8 r, g, b;
		if(hex_to_rgb(hex_buf1, r, g, b)) {
			inp_r1.set_value(r); inp_g1.set_value(g); inp_b1.set_value(b);
			preview_1.set_color(make_rgb_pixval(r, g, b));
		}
		return true;
	}
	if(comp == &inp_hex2) {
		uint8 r, g, b;
		if(hex_to_rgb(hex_buf2, r, g, b)) {
			inp_r2.set_value(r); inp_g2.set_value(g); inp_b2.set_value(b);
			preview_2.set_color(make_rgb_pixval(r, g, b));
		}
		return true;
	}
	// OS color picker buttons
	if(comp == &bt_os_picker_1) {
		uint8 r = (uint8)inp_r1.get_value(), g = (uint8)inp_g1.get_value(), b = (uint8)inp_b1.get_value();
		if(dr_pick_color_start(r, g, b)) {
			pending_os_pick = 0;
		}
		else {
			create_win( new news_img("A color picker is already open."), w_time_delete, magic_none );
		}
		return true;
	}
	if(comp == &bt_os_picker_2) {
		uint8 r = (uint8)inp_r2.get_value(), g = (uint8)inp_g2.get_value(), b = (uint8)inp_b2.get_value();
		if(dr_pick_color_start(r, g, b)) {
			pending_os_pick = 1;
		}
		else {
			create_win( new news_img("A color picker is already open."), w_time_delete, magic_none );
		}
		return true;
	}

	for(unsigned i=0;  i<28;  i++) {

		// new player 1 color?
		if(comp==player_color_1[i]) {
			for(unsigned j=0;  j<28;  j++) {
				player_color_1[j]->pressed = false;
				player_color_2[j]->pressed = false;
			}
			player_color_1[i]->pressed = true;

			// Also update color2 inputs to current values so Apply works consistently
			uint8 r2, g2, b2;
			if(player->is_player_color_custom()) {
				r2 = player->get_player_color_r(1);
				g2 = player->get_player_color_g(1);
				b2 = player->get_player_color_b(1);
			}
			else {
				pixval_to_rgb8(player->get_player_color2_pixval(env_t::gui_player_color_bright), r2, g2, b2);
			}
			inp_r2.set_value(r2); inp_g2.set_value(g2); inp_b2.set_value(b2);
			preview_2.set_color(make_rgb_pixval(r2, g2, b2));

			// Use preset palette index (not custom RGB)
			cbuffer_t buf;
			buf.printf( "1%u,%i", player->get_player_nr(), i*8);
			tool_t *w = create_tool( TOOL_RECOLOUR_TOOL | SIMPLE_TOOL );
			w->set_default_param( buf );
			world()->set_tool( w, player );
			delete w;

			// Sync inp_r1/g1/b1 to chosen preset color
			uint8 r1, g1, b1;
			pixval_to_rgb8(color_idx_to_rgb(i*8 + env_t::gui_player_color_bright), r1, g1, b1);
			inp_r1.set_value(r1); inp_g1.set_value(g1); inp_b1.set_value(b1);
			preview_1.set_color(make_rgb_pixval(r1, g1, b1));
			return true;
		}

		// new player color 2?
		if(comp==player_color_2[i]) {
			for(unsigned j=0;  j<28;  j++) {
				player_color_1[j]->pressed = false;
				player_color_2[j]->pressed = false;
			}
			player_color_2[i]->pressed = true;

			uint8 r1, g1, b1;
			if(player->is_player_color_custom()) {
				r1 = player->get_player_color_r(0);
				g1 = player->get_player_color_g(0);
				b1 = player->get_player_color_b(0);
			}
			else {
				pixval_to_rgb8(player->get_player_color1_pixval(env_t::gui_player_color_bright), r1, g1, b1);
			}
			inp_r1.set_value(r1); inp_g1.set_value(g1); inp_b1.set_value(b1);
			preview_1.set_color(make_rgb_pixval(r1, g1, b1));

			cbuffer_t buf;
			buf.printf( "2%u,%i", player->get_player_nr(), i*8);
			tool_t *w = create_tool( TOOL_RECOLOUR_TOOL | SIMPLE_TOOL );
			w->set_default_param( buf );
			world()->set_tool( w, player );
			delete w;

			uint8 r2, g2, b2;
			pixval_to_rgb8(color_idx_to_rgb(i*8 + env_t::gui_player_color_bright), r2, g2, b2);
			inp_r2.set_value(r2); inp_g2.set_value(g2); inp_b2.set_value(b2);
			preview_2.set_color(make_rgb_pixval(r2, g2, b2));
			return true;
		}

		if(comp==&bt_all_line_color_change) {
			if(  player==welt->get_active_player() && !player->is_locked()  ) {
				cbuffer_t buf;
				buf.printf( "O,%i,%i", 0, player->get_player_nr());
				tool_t* w = create_tool( TOOL_CHANGE_LINE | SIMPLE_TOOL );
				w->set_default_param(buf);
				world()->set_tool( w, player );

				delete w;

				return true;
			}
		}

	}

	return false;
}


void farbengui_t::draw(scr_coord pos, scr_size size)
{
	if(pending_os_pick >= 0) {
		uint8 r, g, b;
		const color_pick_result_t res = dr_pick_color_poll(r, g, b);
		if(res == COLOR_PICK_OK) {
			if(pending_os_pick == 0) {
				inp_r1.set_value(r); inp_g1.set_value(g); inp_b1.set_value(b);
				preview_1.set_color(make_rgb_pixval(r, g, b));
				rgb_to_hex(r, g, b, hex_buf1);
			}
			else {
				inp_r2.set_value(r); inp_g2.set_value(g); inp_b2.set_value(b);
				preview_2.set_color(make_rgb_pixval(r, g, b));
				rgb_to_hex(r, g, b, hex_buf2);
			}
		}
		if(res != COLOR_PICK_RUNNING) {
			pending_os_pick = -1;
		}
	}
	gui_frame_t::draw(pos, size);
}
