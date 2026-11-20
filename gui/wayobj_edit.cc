/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../simworld.h"
#include "../simmenu.h"

#include "../obj/wayobj.h"

#include "../descriptor/way_obj_desc.h"
#include "../descriptor/skin_desc.h"

#include "../dataobj/translator.h"

#include "../utils/cbuffer_t.h"

#include "wayobj_edit.h"


wayobj_edit_frame_t::wayobj_edit_frame_t(player_t* player_, waytype_t wt) :
	infra_edit_frame_t("wayobj builder", player_, wt)
{
	add_filter_number( ni_min_speed,       "Min. speed",       0, 9999, 10 );
	add_filter_number( ni_max_speed,       "Max. speed",       0, 9999, 10, "0 = no limit" );
	add_filter_number( ni_max_cost,        "Max. cost",        0, 9999999, 1, "0 = no limit" );
	add_filter_number( ni_max_maintenance, "Max. maintenance", 0, 9999999, 1, "0 = no limit" );

	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_SPEED);
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_MAINTENANCE);
	set_sortedby(gui_sorting_item_t::BY_SPEED);

	fill_list();
	reset_min_windowsize();
}


void wayobj_edit_frame_t::collect_entries()
{
	const sint32 min_speed = ni_min_speed.get_value();
	const sint32 max_speed = ni_max_speed.get_value();

	FOR(stringhashtable_tpl<way_obj_desc_t const*>, const& i, wayobj_t::get_list()) {
		way_obj_desc_t const* const desc = i.value;
		if(  desc->get_builder() == NULL  ||  !is_waytype_ok( desc->get_wtyp() )  ) {
			continue;
		}
		infra_entry_t e;
		e.desc        = desc;
		e.name        = desc->get_name();
		e.copyright   = desc->get_copyright();
		e.tool        = desc->get_builder();
		e.icon        = desc->get_cursor() ? desc->get_cursor()->get_image_id(1) : IMG_EMPTY;
		e.cursor      = desc->get_cursor() ? desc->get_cursor()->get_image_id(0) : IMG_EMPTY;
		e.price       = desc->get_price();
		e.maintenance = desc->get_maintenance();
		e.speed       = desc->get_topspeed();
		e.intro_date  = desc->get_intro_year_month();
		e.retire_date = desc->get_retire_year_month();
		e.color       = SYSCOL_TEXT;
		if(  e.speed >= min_speed
			&&  (max_speed <= 0  ||  e.speed <= max_speed)
			&&  is_below_limit( e.price, ni_max_cost.get_value() )
			&&  is_below_limit( get_monthly_maintenance(e.maintenance), ni_max_maintenance.get_value() )
			&&  is_timeline_ok( desc )
			&&  is_name_ok( e.name )  ) {
			entries.append( e );
		}
	}
}


void wayobj_edit_frame_t::fill_image_choices(const infra_entry_t &e)
{
	const way_obj_desc_t *desc = static_cast<const way_obj_desc_t *>(e.desc);
	char name[64];
	for(  ribi_t::ribi r = 0;  r < 16;  r++  ) {
		sprintf( name, "image[%s]", infra_ribi_codes[r] );
		add_image_choice( name, desc->get_back_image_id(r), desc->get_front_image_id(r) );
	}
	if(  desc->has_switch_image()  ) {
		for(  uint8 nw = 0;  nw < 2;  nw++  ) {
			for(  uint8 k = 0;  k < 5;  k++  ) {
				sprintf( name, "image[%s]", infra_ribi_codes[16 + k + nw*5] );
				add_image_choice( name, desc->get_crossing_image_id(infra_switch_ribis[k], nw, false), desc->get_crossing_image_id(infra_switch_ribis[k], nw, true) );
			}
		}
	}
	for(  uint8 h = 1;  h <= 2;  h++  ) {
		for(  uint8 k = 0;  k < 4;  k++  ) {
			const slope_t::type sl = (slope_t::type)(infra_up_slopes[k] * h);
			sprintf( name, h == 1 ? "imageup[%d]" : "imageup2[%d]", infra_up_slope_numbers[k] );
			add_image_choice( name, desc->get_back_slope_image_id(sl), desc->get_front_slope_image_id(sl) );
		}
	}
	if(  desc->has_diagonal_image()  ) {
		for(  uint8 k = 0;  k < 4;  k++  ) {
			sprintf( name, "diagonal[%s]", infra_ribi_codes[infra_diagonal_ribis[k]] );
			add_image_choice( name, desc->get_back_diagonal_image_id(infra_diagonal_ribis[k]), desc->get_front_diagonal_image_id(infra_diagonal_ribis[k]) );
		}
	}
	if(  desc->has_close_diagonal_image()  ) {
		for(  uint8 k = 0;  k < 2;  k++  ) {
			sprintf( name, "diagonal[close%d]", k+1 );
			add_image_choice( name, desc->get_back_close_diagonal_image_id(k), desc->get_front_close_diagonal_image_id(k) );
		}
	}
}


void wayobj_edit_frame_t::fill_all_image_names()
{
	add_way_image_names();
}


void wayobj_edit_frame_t::append_info(const infra_entry_t &e)
{
	const way_obj_desc_t *desc = static_cast<const way_obj_desc_t *>(e.desc);
	if(  desc->is_overhead_line()  ) {
		buf.printf( "%s\n", translator::translate("overheadlines") );
	}
	buf.printf( "%s: %d km/h\n", translator::translate("Max. speed"), e.speed );
}
