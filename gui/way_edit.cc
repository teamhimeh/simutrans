/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../simworld.h"
#include "../simmenu.h"

#include "../bauer/wegbauer.h"
#include "../bauer/brueckenbauer.h"
#include "../bauer/tunnelbauer.h"

#include "../descriptor/way_desc.h"
#include "../descriptor/bridge_desc.h"
#include "../descriptor/tunnel_desc.h"
#include "../descriptor/skin_desc.h"

#include "../dataobj/translator.h"
#include "../dataobj/environment.h"
#include "../dataobj/ribi.h"

#include "../utils/cbuffer_t.h"

#include "way_edit.h"


/// bridge image names (same order as bridge_desc_t::img_t)
static const char* const bridge_image_names[24] = {
	"image[ns]",   "image[ew]",
	"start[n]",    "start[s]",  "start[e]",  "start[w]",
	"ramp[n]",     "ramp[s]",   "ramp[e]",   "ramp[w]",
	"pillar[s]",   "pillar[w]",
	"image2[ns]",  "image2[ew]",
	"start2[n]",   "start2[s]", "start2[e]", "start2[w]",
	"ramp2[n]",    "ramp2[s]",  "ramp2[e]",  "ramp2[w]",
	"pillar2[s]",  "pillar2[w]"
};

/// tunnel portal names and their slopes
static const char* const tunnel_dirs[4] = { "n", "s", "e", "w" };
static const char* const tunnel_portal_types[4] = { "", "l", "r", "m" };
static const slope_t::type tunnel_slopes[4] = { slope_t::south, slope_t::north, slope_t::west, slope_t::east };


way_edit_frame_t::way_edit_frame_t(player_t* player_, waytype_t wt) :
	infra_edit_frame_t("way builder", player_, wt)
{
	add_filter_button( bt_ground,   "show ground",   true );
	add_filter_button( bt_elevated, "show elevated", true );
	add_filter_button( bt_bridge,   "show bridge",   true );
	add_filter_button( bt_tunnel,   "show tunnel",   true );

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


bool way_edit_frame_t::is_way_waytype_ok(waytype_t wt, bool tram) const
{
	if(  filter_waytype == ignore_wt  ) {
		return true;
	}
	if(  tram  ) {
		return filter_waytype == tram_wt;
	}
	return wt == filter_waytype;
}


bool way_edit_frame_t::is_common_filter_ok(const infra_entry_t &e)
{
	const sint32 max_speed = ni_max_speed.get_value();
	return  e.speed >= ni_min_speed.get_value()
		&&  (max_speed <= 0  ||  e.speed <= max_speed)
		&&  is_below_limit( e.price, ni_max_cost.get_value() )
		&&  is_below_limit( get_monthly_maintenance(e.maintenance), ni_max_maintenance.get_value() )
		&&  is_timeline_ok( e.desc )
		&&  is_name_ok( e.name );
}


static image_id cursor_icon(const skin_desc_t *cursor)
{
	return cursor ? cursor->get_image_id(1) : IMG_EMPTY;
}


void way_edit_frame_t::collect_entries()
{
	if(  bt_ground.pressed  ||  bt_elevated.pressed  ) {
		FOR(stringhashtable_tpl<way_desc_t const*>, const& i, way_builder_t::get_desc_table()) {
			way_desc_t const* const desc = i.value;
			if(  desc->get_builder() == NULL  ||  !is_way_waytype_ok( desc->get_wtyp(), desc->is_tram() )  ) {
				continue;
			}
			const bool elevated = desc->get_styp() == type_elevated  &&  desc->get_wtyp() != air_wt;
			if(  elevated ? !bt_elevated.pressed : !bt_ground.pressed  ) {
				continue;
			}
			infra_entry_t e;
			e.desc        = desc;
			e.name        = desc->get_name();
			e.copyright   = desc->get_copyright();
			e.tool        = desc->get_builder();
			e.icon        = cursor_icon( desc->get_cursor() );
			e.cursor      = desc->get_cursor() ? desc->get_cursor()->get_image_id(0) : IMG_EMPTY;
			e.price       = desc->get_price();
			e.maintenance = desc->get_maintenance();
			e.speed       = desc->get_topspeed();
			e.intro_date  = desc->get_intro_year_month();
			e.retire_date = desc->get_retire_year_month();
			e.kind        = elevated ? KIND_ELEVATED : KIND_GROUND;
			e.color       = elevated ? color_idx_to_rgb(COL_DARK_GREEN + env_t::gui_player_color_dark) : SYSCOL_TEXT;
			if(  is_common_filter_ok(e)  ) {
				entries.append( e );
			}
		}
	}

	if(  bt_bridge.pressed  ) {
		FOR(stringhashtable_tpl<bridge_desc_t const*>, const& i, bridge_builder_t::get_desc_table()) {
			bridge_desc_t const* const desc = i.value;
			if(  desc->get_builder() == NULL  ||  !is_waytype_ok( desc->get_waytype() )  ) {
				continue;
			}
			infra_entry_t e;
			e.desc        = desc;
			e.name        = desc->get_name();
			e.copyright   = desc->get_copyright();
			e.tool        = desc->get_builder();
			e.icon        = cursor_icon( desc->get_cursor() );
			e.cursor      = desc->get_cursor() ? desc->get_cursor()->get_image_id(0) : IMG_EMPTY;
			e.price       = desc->get_price();
			e.maintenance = desc->get_maintenance();
			e.speed       = desc->get_topspeed();
			e.intro_date  = desc->get_intro_year_month();
			e.retire_date = desc->get_retire_year_month();
			e.kind        = KIND_BRIDGE;
			e.color       = color_idx_to_rgb(COL_DARK_BLUE + env_t::gui_player_color_dark);
			if(  is_common_filter_ok(e)  ) {
				entries.append( e );
			}
		}
	}

	if(  bt_tunnel.pressed  ) {
		FOR(stringhashtable_tpl<tunnel_desc_t *>, const& i, tunnel_builder_t::get_desc_table()) {
			tunnel_desc_t const* const desc = i.value;
			if(  desc->get_builder() == NULL  ||  !is_waytype_ok( desc->get_waytype() )  ) {
				continue;
			}
			infra_entry_t e;
			e.desc        = desc;
			e.name        = desc->get_name();
			e.copyright   = desc->get_copyright();
			e.tool        = desc->get_builder();
			e.icon        = cursor_icon( desc->get_cursor() );
			e.cursor      = desc->get_cursor() ? desc->get_cursor()->get_image_id(0) : IMG_EMPTY;
			e.price       = desc->get_price();
			e.maintenance = desc->get_maintenance();
			e.speed       = desc->get_topspeed();
			e.intro_date  = desc->get_intro_year_month();
			e.retire_date = desc->get_retire_year_month();
			e.kind        = KIND_TUNNEL;
			e.color       = color_idx_to_rgb(40 + env_t::gui_player_color_dark);
			if(  is_common_filter_ok(e)  ) {
				entries.append( e );
			}
		}
	}
}


void way_edit_frame_t::fill_image_choices(const infra_entry_t &e)
{
	char name[64];
	switch(  e.kind  ) {
		case KIND_GROUND:
		case KIND_ELEVATED: {
			const way_desc_t *desc = static_cast<const way_desc_t *>(e.desc);
			for(  ribi_t::ribi r = 0;  r < 16;  r++  ) {
				sprintf( name, "image[%s]", infra_ribi_codes[r] );
				add_image_choice( name, desc->get_image_id(r, 0, false), desc->get_image_id(r, 0, true) );
			}
			if(  desc->has_switch_image()  ) {
				for(  uint8 nw = 0;  nw < 2;  nw++  ) {
					for(  uint8 k = 0;  k < 5;  k++  ) {
						sprintf( name, "image[%s]", infra_ribi_codes[16 + k + nw*5] );
						add_image_choice( name, desc->get_switch_image_id(infra_switch_ribis[k], 0, nw), desc->get_switch_image_id(infra_switch_ribis[k], 0, nw, true) );
					}
				}
			}
			for(  uint8 h = 1;  h <= 2;  h++  ) {
				for(  uint8 k = 0;  k < 4;  k++  ) {
					const slope_t::type sl = (slope_t::type)(infra_up_slopes[k] * h);
					sprintf( name, h == 1 ? "imageup[%d]" : "imageup2[%d]", infra_up_slope_numbers[k] );
					add_image_choice( name, desc->get_slope_image_id(sl, 0, false), desc->get_slope_image_id(sl, 0, true) );
				}
			}
			if(  desc->has_diagonal_image()  ) {
				for(  uint8 k = 0;  k < 4;  k++  ) {
					sprintf( name, "diagonal[%s]", infra_ribi_codes[infra_diagonal_ribis[k]] );
					add_image_choice( name, desc->get_diagonal_image_id(infra_diagonal_ribis[k], 0, false), desc->get_diagonal_image_id(infra_diagonal_ribis[k], 0, true) );
				}
			}
			if(  desc->has_close_diagonal_image()  ) {
				for(  uint8 k = 0;  k < 2;  k++  ) {
					sprintf( name, "diagonal[close%d]", k+1 );
					add_image_choice( name, desc->get_close_diagonal_image_id(k, 0, false), desc->get_close_diagonal_image_id(k, 0, true) );
				}
			}
			break;
		}

		case KIND_BRIDGE: {
			const bridge_desc_t *desc = static_cast<const bridge_desc_t *>(e.desc);
			for(  int k = 0;  k < 24;  k++  ) {
				const bridge_desc_t::img_t img = (bridge_desc_t::img_t)k;
				const image_id back  = desc->get_background(img, 0);
				const image_id front = desc->get_foreground(img, 0);
				if(  k >= bridge_desc_t::NS_Segment2  &&  back == IMG_EMPTY  &&  front == IMG_EMPTY  ) {
					// optional double height images
					continue;
				}
				add_image_choice( bridge_image_names[k], back, front );
			}
			break;
		}

		case KIND_TUNNEL: {
			const tunnel_desc_t *desc = static_cast<const tunnel_desc_t *>(e.desc);
			const uint8 portal_types = desc->has_broad_portals() ? 4 : 1;
			for(  uint8 j = 0;  j < portal_types;  j++  ) {
				for(  uint8 k = 0;  k < 4;  k++  ) {
					sprintf( name, "image[%s%s]", tunnel_dirs[k], tunnel_portal_types[j] );
					add_image_choice( name, desc->get_background_id(tunnel_slopes[k], 0, j), desc->get_foreground_id(tunnel_slopes[k], 0, j) );
				}
			}
			break;
		}
	}
}


void way_edit_frame_t::fill_all_image_names()
{
	add_way_image_names();
	for(  int k = 0;  k < 24;  k++  ) {
		add_image_choice( bridge_image_names[k], IMG_EMPTY, IMG_EMPTY );
	}
	char name[64];
	for(  uint8 j = 0;  j < 4;  j++  ) {
		for(  uint8 k = 0;  k < 4;  k++  ) {
			sprintf( name, "image[%s%s]", tunnel_dirs[k], tunnel_portal_types[j] );
			add_image_choice( name, IMG_EMPTY, IMG_EMPTY );
		}
	}
}


void way_edit_frame_t::append_info(const infra_entry_t &e)
{
	switch(  e.kind  ) {
		case KIND_GROUND:   buf.printf( "%s\n", translator::translate("Way") );          break;
		case KIND_ELEVATED: buf.printf( "%s\n", translator::translate("Elevated way") ); break;
		case KIND_BRIDGE:   buf.printf( "%s\n", translator::translate("Bridge") );       break;
		case KIND_TUNNEL:   buf.printf( "%s\n", translator::translate("Tunnel") );       break;
	}
	buf.printf( "%s: %d km/h\n", translator::translate("Max. speed"), e.speed );

	const obj_desc_transport_infrastructure_t *desc = static_cast<const obj_desc_transport_infrastructure_t *>(e.desc);
	if(  desc->get_axle_load() > 0  ) {
		buf.printf( "%s: %dt\n", translator::translate("Max. axle load"), desc->get_axle_load() );
	}

	if(  e.kind == KIND_BRIDGE  ) {
		const bridge_desc_t *b = static_cast<const bridge_desc_t *>(e.desc);
		if(  b->get_max_length() > 0  ) {
			buf.printf( "%s: %d\n", translator::translate("Max. length"), b->get_max_length() );
		}
		if(  b->get_max_height() > 0  ) {
			buf.printf( "%s: %d\n", translator::translate("Max. height"), b->get_max_height() );
		}
	}
	else if(  e.kind == KIND_TUNNEL  ) {
		const tunnel_desc_t *t = static_cast<const tunnel_desc_t *>(e.desc);
		if(  const way_desc_t *w = t->get_way_desc()  ) {
			buf.printf( "%s: %s\n", translator::translate("Way"), translator::translate(w->get_name()) );
		}
	}
}
