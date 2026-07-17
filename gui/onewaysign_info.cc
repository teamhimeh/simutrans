/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include "onewaysign_info.h"
#include "components/gui_label.h"
#include "../obj/roadsign.h"
#include "../player/simplay.h"
#include "../dataobj/ribi.h"
#include "../dataobj/translator.h"
#include "../boden/grund.h"
#include "../boden/wege/weg.h"

#include "../simmenu.h"
#include "../simworld.h"

// Labels: which edge the vehicle is entering FROM (ribi_t::nesw order: N, E, S, W).
static const char* const from_label[4] = { "from S", "from W", "from N", "from E" };
// Exit direction column headers (same order).
static const char* const exit_label[4] = { "N", "E", "S", "W" };


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
					if(  !is_multi_way  ||  !sign->is_detailed_oneway()  ) {
						bt_exit[row][col].disable();
					}
					add_component(&bt_exit[row][col]);
				}
			}
		}
		end_table();
	}

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


bool onewaysign_info_t::action_triggered(gui_action_creator_t *komp, value_t)
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

	// Exit ribi checkboxes — only functional when detailed_oneway is enabled
	if(  sign->is_detailed_oneway()  ) {
		for(  int row = 0;  row < 4;  row++  ) {
			ribi_t::ribi entry = ribi_t::nesw[row];
			for(  int col = 0;  col < 4;  col++  ) {
				if(  komp == &bt_exit[row][col]  ) {
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
