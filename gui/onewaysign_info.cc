/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <math.h>

#include "onewaysign_info.h"
#include "../macros.h"
#include "components/gui_label.h"
#include "../obj/roadsign.h"
#include "../player/simplay.h"
#include "../dataobj/ribi.h"
#include "../dataobj/translator.h"
#include "../boden/grund.h"
#include "../boden/wege/weg.h"
#include "../simcolor.h"
#include "../display/simgraph.h"

#include "../simmenu.h"
#include "../simworld.h"

// Labels: which edge the vehicle is entering FROM (ribi_t::nesw order: N, E, S, W).
static const char* const from_label[4] = { "from S", "from W", "from N", "from E" };
// Exit direction column headers (same order).
static const char* const exit_label[4] = { "N", "E", "S", "W" };

// nesw index (N=0,E=1,S=2,W=3) of the edge a vehicle enters through, per info-window row.
static const int entry_edge_of_row[4] = { 2, 3, 0, 1 };
// nesw index of the edge a vehicle leaves through, per exit column.
static const int exit_edge_of_col[4]  = { 0, 1, 2, 3 };


// screen point (local to the diagram) where an arrow touches the given edge
static scr_coord diagram_edge_point( int edge, bool is_exit, scr_coord_val S )
{
	const int lo  = S * 6 / 100;
	const int hi  = S - lo;
	const int mid = S / 2;
	const int o   = S * 9 / 100;
	const int d   = is_exit ? o : -o;
	switch(  edge  ) {
		case 0: return scr_coord(mid + d, lo); // N
		case 1: return scr_coord(hi, mid + d); // E
		case 2: return scr_coord(mid - d, hi); // S
		case 3: return scr_coord(lo, mid - d); // W
	}
	return scr_coord(mid, mid);
}


// a straight line of the given pixel thickness (built from parallel 1px lines)
static void display_thick_line( scr_coord a, scr_coord b, PIXVAL col, int thickness )
{
	if(  thickness < 1  ) {
		thickness = 1;
	}
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	const double len = sqrt( dx*dx + dy*dy );
	double px = 0.0, py = 0.0;
	if(  len > 0.5  ) {
		px = -dy / len;
		py =  dx / len;
	}
	for(  int i = 0;  i < thickness;  i++  ) {
		const double t = i - (thickness - 1) / 2.0;
		const int ox = (int)floor( t * px + 0.5 );
		const int oy = (int)floor( t * py + 0.5 );
		display_direct_line_rgb( a.x + ox, a.y + oy, b.x + ox, b.y + oy, col );
	}
}


// squared distance from point p to segment a-b
static double seg_dist2( scr_coord p, scr_coord a, scr_coord b )
{
	const double vx = b.x - a.x;
	const double vy = b.y - a.y;
	const double wx = p.x - a.x;
	const double wy = p.y - a.y;
	const double len2 = vx*vx + vy*vy;
	double t = len2 > 0.0 ? (wx*vx + wy*vy) / len2 : 0.0;
	if(  t < 0.0  ) t = 0.0;
	if(  t > 1.0  ) t = 1.0;
	const double dx = a.x + t*vx - p.x;
	const double dy = a.y + t*vy - p.y;
	return dx*dx + dy*dy;
}


bool gui_oneway_diagram_t::is_uturn( int row, int col )
{
	return entry_edge_of_row[row] == exit_edge_of_col[col];
}


void gui_oneway_diagram_t::get_arrow( int row, int col, scr_coord &a, scr_coord &b ) const
{
	const scr_coord_val S = min( get_size().w, get_size().h );
	a = diagram_edge_point( entry_edge_of_row[row], false, S );
	b = diagram_edge_point( exit_edge_of_col[col],  true,  S );
}


scr_size gui_oneway_diagram_t::get_min_size() const
{
	const scr_coord_val s = 9 * LINESPACE;
	return scr_size(s, s);
}


void gui_oneway_diagram_t::draw( scr_coord offset )
{
	if(  sign == NULL  ) {
		return;
	}

	const scr_coord base = pos + offset;
	const scr_coord_val S = min( get_size().w, get_size().h );
	const int mid = S / 2;
	const int rw  = S * 13 / 100;

	// the two carriageways of the junction
	const PIXVAL road_col = color_idx_to_rgb(MN_GREY1);
	display_fillbox_wh_clip_rgb( base.x + mid - rw, base.y,          2*rw, S,    road_col, false );
	display_fillbox_wh_clip_rgb( base.x,            base.y + mid - rw, S,   2*rw, road_col, false );

	const PIXVAL col_yes   = color_idx_to_rgb( active ? COL_GREEN : MN_GREY0 );
	const PIXVAL col_no    = color_idx_to_rgb( COL_GREY3 );

	for(  int row = 0;  row < 4;  row++  ) {
		const ribi_t::ribi allowed = sign->get_detailed_oneway_out_ribi( ribi_t::nesw[row] );
		for(  int col = 0;  col < 4;  col++  ) {
			if(  is_uturn(row, col)  ) {
				continue;
			}
			const bool yes = (allowed & ribi_t::nesw[col]) != 0;
			if(  !active  &&  !yes  ) {
				continue; // keep the picture clean while editing is disabled
			}
			const PIXVAL acol = yes ? col_yes : col_no;

			scr_coord a, b;
			get_arrow( row, col, a, b );
			a += base;
			b += base;
			const int thick = max( 3, S * 4 / 100 );
			display_thick_line( a, b, acol, thick );

			// arrow head at the exit end
			double dx = b.x - a.x;
			double dy = b.y - a.y;
			const double len = sqrt( dx*dx + dy*dy );
			if(  len > 0.5  ) {
				dx /= len;
				dy /= len;
				const double hl = S * 0.13;
				const double hw = S * 0.07;
				const scr_coord l( (int)( b.x - dx*hl - dy*hw ), (int)( b.y - dy*hl + dx*hw ) );
				const scr_coord r( (int)( b.x - dx*hl + dy*hw ), (int)( b.y - dy*hl - dx*hw ) );
				display_thick_line( b, l, acol, thick );
				display_thick_line( b, r, acol, thick );
			}
		}
	}
}


bool gui_oneway_diagram_t::infowin_event( const event_t *ev )
{
	if(  !active  ||  sign == NULL  ||  !IS_LEFTRELEASE(ev)  ) {
		return false;
	}

	const scr_coord_val S = min( get_size().w, get_size().h );
	const scr_coord p( ev->cx, ev->cy );

	double thresh = S * 0.06;
	if(  thresh < 4.0  ) {
		thresh = 4.0;
	}
	double best_d2 = thresh * thresh;
	int best = -1;

	for(  int row = 0;  row < 4;  row++  ) {
		for(  int col = 0;  col < 4;  col++  ) {
			if(  is_uturn(row, col)  ) {
				continue;
			}
			scr_coord a, b;
			get_arrow( row, col, a, b );
			const double d2 = seg_dist2( p, a, b );
			if(  d2 < best_d2  ) {
				best_d2 = d2;
				best = row * 4 + col;
			}
		}
	}

	if(  best >= 0  ) {
		call_listeners( value_t( (long)best ) );
		return true;
	}
	return false;
}


// ---------------------------------------------------------------------------


uint8 onewaysign_info_t::clip_ns = 0;
uint8 onewaysign_info_t::clip_ow = 0;
bool  onewaysign_info_t::clip_valid = false;


onewaysign_info_t::onewaysign_info_t(roadsign_t* s, koord3d first_intersection) :
	obj_infowin_t(s),
	sign(s)
{
	has_intersection = (first_intersection != koord3d::invalid);
	has_choose = sign->get_desc()->is_choose_sign();

	// Detailed oneway only makes sense at 3/4-way junctions.
	{
		const grund_t *gr = welt->lookup(sign->get_pos());
		const weg_t *weg = gr ? gr->get_weg(sign->get_desc()->get_wtyp() != tram_wt
		                         ? sign->get_desc()->get_wtyp() : track_wt) : NULL;
		const ribi_t::ribi way_ribi = weg ? weg->get_ribi_unmasked() : ribi_t::none;
		is_multi_way = !ribi_t::is_single(way_ribi) && !ribi_t::is_twoway(way_ribi);
	}

	set_table_layout(1, 0);

	// Lane affinity (road only, only when an intersection was found)
	if(  sign->get_desc()->is_single_way()  &&  has_intersection  ) {
		direction[0].init(button_t::square_state, translator::translate("Left"));
		direction[1].init(button_t::square_state, translator::translate("Right"));
		direction[0].add_listener(this);
		direction[1].add_listener(this);
		direction[0].pressed = (sign->get_lane_affinity() & 1) != 0;
		direction[1].pressed = (sign->get_lane_affinity() & 2) != 0;
		add_table(2, 1);
		{
			add_component(&direction[0]);
			add_component(&direction[1]);
		}
		end_table();
	}

	// Length-based choose (choose signs only)
	if(  has_choose  ) {
		bt_length_based.init(button_t::square_state, translator::translate("Length based choose"));
		bt_length_based.add_listener(this);
		bt_length_based.pressed = sign->is_length_based();
		add_component(&bt_length_based);
	}

	// Detailed oneway section (single_way signs only)
	if(  sign->get_desc()->is_single_way()  ) {
		bt_detailed_oneway.init(button_t::square_state, translator::translate("Detailed oneway"));
		bt_detailed_oneway.add_listener(this);
		bt_detailed_oneway.pressed = sign->is_detailed_oneway();
		if(  !is_multi_way  ) {
			bt_detailed_oneway.disable();
		}
		add_component(&bt_detailed_oneway);

		// Clickable junction diagram: an arrow per allowed/forbidden turn.
		const bool detail_active = is_multi_way && sign->is_detailed_oneway();
		diagram.init(sign);
		diagram.set_active(detail_active);
		diagram.add_listener(this);
		add_component(&diagram);

		new_component<gui_label_t>(translator::translate("Click an arrow to toggle whether that turn is allowed"));

		// 4x5 grid: one row per entry direction (N, E, S, W), with label + 4 exit checkboxes.
		// The grid is always present; buttons are inert when detailed_oneway is off.
		add_table(5, 4);
		{
			for(  int row = 0;  row < 4;  row++  ) {
				ribi_t::ribi entry = ribi_t::nesw[row];
				ribi_t::ribi allowed = sign->get_detailed_oneway_out_ribi(entry);
				new_component<gui_label_t>(translator::translate(from_label[row]));
				for(  int col = 0;  col < 4;  col++  ) {
					ribi_t::ribi exit_r = ribi_t::nesw[col];
					char tooltip[32];
					sprintf(tooltip, "%s->%s", translator::translate(from_label[row]), exit_label[col]);
					bt_exit[row][col].init(button_t::square_state, exit_label[col]);
					bt_exit[row][col].set_tooltip(tooltip);
					bt_exit[row][col].add_listener(this);
					bt_exit[row][col].pressed = (allowed & exit_r) != 0;
					if(  !detail_active  ) {
						bt_exit[row][col].disable();
					}
					add_component(&bt_exit[row][col]);
				}
			}
		}
		end_table();

		// Copy / paste the whole turn matrix to another one-way sign.
		add_table(2, 1);
		{
			bt_copy.init(button_t::roundbox, translator::translate("Copy oneway detail"));
			bt_copy.add_listener(this);
			bt_copy.enable(sign->is_detailed_oneway());
			add_component(&bt_copy);

			bt_paste.init(button_t::roundbox, translator::translate("Paste oneway detail"));
			bt_paste.add_listener(this);
			bt_paste.enable(clip_valid && is_multi_way);
			add_component(&bt_paste);
		}
		end_table();
	}

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


void onewaysign_info_t::apply_exit_toggle(int row, int col)
{
	ribi_t::ribi entry = ribi_t::nesw[row];
	ribi_t::ribi exit_r = ribi_t::nesw[col];
	ribi_t::ribi allowed = sign->get_detailed_oneway_out_ribi(entry);
	allowed = (ribi_t::ribi)(allowed ^ exit_r); // toggle this exit bit
	char param[256];
	if(  entry == ribi_t::north  ||  entry == ribi_t::south  ) {
		// pack: N-entry in bits 0-3, S-entry in bits 4-7
		uint8 cur_ns = (uint8)(  (sign->get_detailed_oneway_out_ribi(ribi_t::north) & 0xF)
		                       | ((sign->get_detailed_oneway_out_ribi(ribi_t::south) & 0xF) << 4) );
		if(  entry == ribi_t::north  ) {
			cur_ns = (uint8)((cur_ns & 0xF0) | (allowed & 0xF));
		} else {
			cur_ns = (uint8)((cur_ns & 0x0F) | ((allowed & 0xF) << 4));
		}
		sprintf(param, "%s,%i,n", sign->get_pos().get_str(), (int)cur_ns);
	} else {
		// pack: E-entry in bits 0-3, W-entry in bits 4-7
		uint8 cur_ow = (uint8)(  (sign->get_detailed_oneway_out_ribi(ribi_t::east) & 0xF)
		                       | ((sign->get_detailed_oneway_out_ribi(ribi_t::west) & 0xF) << 4) );
		if(  entry == ribi_t::east  ) {
			cur_ow = (uint8)((cur_ow & 0xF0) | (allowed & 0xF));
		} else {
			cur_ow = (uint8)((cur_ow & 0x0F) | ((allowed & 0xF) << 4));
		}
		sprintf(param, "%s,%i,e", sign->get_pos().get_str(), (int)cur_ow);
	}
	tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
	welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
}


bool onewaysign_info_t::action_triggered(gui_action_creator_t *komp, value_t extra)
{
	// Length-based choose
	if(  komp == &bt_length_based  ) {
		char param[256];
		sprintf(param, "%s,%i,l", sign->get_pos().get_str(), (int)!sign->is_length_based());
		tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
		welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
		return true;
	}

	// Detailed oneway toggle
	if(  komp == &bt_detailed_oneway  ) {
		char param[256];
		sprintf(param, "%s,%i,D", sign->get_pos().get_str(), (int)!sign->is_detailed_oneway());
		tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
		welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
		return true;
	}

	// Clickable junction diagram
	if(  komp == &diagram  ) {
		if(  sign->is_detailed_oneway()  ) {
			const long v = extra.i;
			apply_exit_toggle((int)(v / 4), (int)(v % 4));
		}
		return true;
	}

	// Copy the whole turn matrix
	if(  komp == &bt_copy  ) {
		clip_ns = (uint8)(  (sign->get_detailed_oneway_out_ribi(ribi_t::north) & 0xF)
		                  | ((sign->get_detailed_oneway_out_ribi(ribi_t::south) & 0xF) << 4) );
		clip_ow = (uint8)(  (sign->get_detailed_oneway_out_ribi(ribi_t::east) & 0xF)
		                  | ((sign->get_detailed_oneway_out_ribi(ribi_t::west) & 0xF) << 4) );
		clip_valid = true;
		bt_paste.enable(is_multi_way);
		return true;
	}

	// Paste a previously copied turn matrix
	if(  komp == &bt_paste  ) {
		if(  clip_valid  &&  is_multi_way  ) {
			char param[256];
			if(  !sign->is_detailed_oneway()  ) {
				sprintf(param, "%s,1,D", sign->get_pos().get_str());
				tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
				welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
			}
			sprintf(param, "%s,%i,n", sign->get_pos().get_str(), (int)clip_ns);
			tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
			welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
			sprintf(param, "%s,%i,e", sign->get_pos().get_str(), (int)clip_ow);
			tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
			welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
		}
		return true;
	}

	// Exit ribi checkboxes — only functional when detailed_oneway is enabled
	if(  sign->is_detailed_oneway()  ) {
		for(  int row = 0;  row < 4;  row++  ) {
			for(  int col = 0;  col < 4;  col++  ) {
				if(  komp == &bt_exit[row][col]  ) {
					apply_exit_toggle(row, col);
					return true;
				}
			}
		}
	}

	// Lane affinity buttons
	if(  sign->get_desc()->is_single_way()  &&  has_intersection  ) {
		uint8 fix = sign->get_lane_affinity();
		for(  int i = 0;  i < 2;  i++  ) {
			if(  komp == &direction[i]  ) {
				fix ^= (uint8)(i + 1);
				char param[256];
				sprintf(param, "%s,%i,r", sign->get_pos().get_str(), (int)fix);
				tool_t::simple_tool[TOOL_CHANGE_ROADSIGN]->set_default_param(param);
				welt->set_tool(tool_t::simple_tool[TOOL_CHANGE_ROADSIGN], welt->get_active_player());
				return true;
			}
		}
	}

	return true;
}


void onewaysign_info_t::update_data()
{
	if(  sign->get_desc()->is_single_way()  &&  has_intersection  ) {
		for(  int i = 0;  i < 2;  i++  ) {
			direction[i].pressed = (sign->get_lane_affinity() & (i + 1)) != 0;
		}
	}
	if(  has_choose  ) {
		bt_length_based.pressed = sign->is_length_based();
	}
	if(  sign->get_desc()->is_single_way()  ) {
		bt_detailed_oneway.pressed = sign->is_detailed_oneway();
		const bool exit_active = is_multi_way && sign->is_detailed_oneway();
		diagram.set_active(exit_active);
		bt_copy.enable(sign->is_detailed_oneway());
		bt_paste.enable(clip_valid && is_multi_way);
		for(  int row = 0;  row < 4;  row++  ) {
			ribi_t::ribi entry = ribi_t::nesw[row];
			ribi_t::ribi allowed = sign->get_detailed_oneway_out_ribi(entry);
			for(  int col = 0;  col < 4;  col++  ) {
				ribi_t::ribi exit_r = ribi_t::nesw[col];
				bt_exit[row][col].pressed = (allowed & exit_r) != 0;
				bt_exit[row][col].enable(exit_active);
			}
		}
	}
}
