/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../simworld.h"
#include "../simmenu.h"
#include "../simtool.h"

#include "../boden/wege/strasse.h"

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


/// overtaking modes in the order of the buttons (same as overtaking_mode_frame_t)
static const overtaking_mode_t overtaking_modes[8] = {
	halt_mode, oneway_mode, twoway_mode, loading_only_mode, prohibited_mode, inverted_mode, exclusive_area_mode, passing_lane_stop_only_mode
};
static const char* const overtaking_mode_names[8] = {
	"halt mode", "oneway", "twoway", "only loading convoi", "prohibited", "inverted", "exclusive area", "passing lane stop only"
};


way_edit_frame_t::way_edit_frame_t(player_t* player_, waytype_t wt) :
	infra_edit_frame_t("way builder", player_, wt),
	opt_way(NULL),
	opt_bridge(NULL),
	opt_tunnel(NULL),
	opt_road(false)
{
	init_tool_options();

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


void way_edit_frame_t::init_tool_options()
{
	cont_options.add_component( &cont_tool_options );
	cont_tool_options.set_table_layout(1,0);
	cont_tool_options.new_component<gui_divider_t>();

	// overtaking mode (roads only)
	cont_tool_options.add_component( &cont_modes );
	cont_modes.set_table_layout(2,0);
	for(  int i = 0;  i < 8;  i++  ) {
		bt_mode[i].init( button_t::square_state, overtaking_mode_names[i] );
		bt_mode[i].add_listener(this);
		cont_modes.add_component( &bt_mode[i] );
	}

	// road flags (roads only)
	cont_tool_options.add_component( &cont_road_flags );
	cont_road_flags.set_table_layout(1,0);
	cont_road_flags.new_component<gui_divider_t>();
	bt_avoid_cityroad.init( button_t::square_state, "avoid becoming cityroad" );
	bt_citycar_no_entry.init( button_t::square_state, "citycars do not enter" );
	bt_no_building.init( button_t::square_state, "no building adjacent" );
	bt_allow_branching_cityroad.init( button_t::square_state, "can branch cityroad" );
	bt_pedestrian_no_entry.init( button_t::square_state, "pedestrians do not enter " );
	button_t *const flags[5] = { &bt_avoid_cityroad, &bt_citycar_no_entry, &bt_no_building, &bt_allow_branching_cityroad, &bt_pedestrian_no_entry };
	for(  int i = 0;  i < 5;  i++  ) {
		flags[i]->add_listener(this);
		cont_road_flags.add_component( flags[i] );
	}
	cont_road_flags.new_component<gui_divider_t>();

	// height offset (elevated ways)
	cont_tool_options.add_component( &cont_height_offset );
	cont_height_offset.set_table_layout(2,0);
	cont_height_offset.new_component<gui_label_t>("height offset");
	ni_height_offset.set_limits( 1-welt->get_settings().get_way_height_clearance(), 32 );
	ni_height_offset.wrap_mode( false );
	ni_height_offset.add_listener(this);
	cont_height_offset.add_component( &ni_height_offset );

	// closed diagonals (ways and tunnels)
	bt_allow_closed_diagonal.init( button_t::square_state, "allow closed diagonal" );
	bt_allow_closed_diagonal.set_tooltip( translator::translate("Allow building closed diagonals (two ways on the opposite diagonals of a tile). The same waytype also needs Ctrl pressed.") );
	bt_allow_closed_diagonal.add_listener(this);
	cont_tool_options.add_component( &bt_allow_closed_diagonal );

	// vehicle offset (ways, bridges and tunnels)
	cont_tool_options.add_component( &cont_vehicle_offset );
	cont_vehicle_offset.set_table_layout(3,0);
	cont_vehicle_offset.new_component<gui_label_t>("vehicle offset");
	ni_vehicle_offset.set_limits( -63, 63 );
	ni_vehicle_offset.wrap_mode( false );
	ni_vehicle_offset.add_listener(this);
	cont_vehicle_offset.add_component( &ni_vehicle_offset );
	bt_vehicle_offset_mode.init( button_t::square_state, "Offset mode: Absolute" );
	bt_vehicle_offset_mode.set_tooltip( translator::translate("set offset value and mode: On->Absolute(same offset for N&S),OFF->Direction(offset different based on Difference)") );
	bt_vehicle_offset_mode.add_listener(this);
	cont_vehicle_offset.add_component( &bt_vehicle_offset_mode );

	cont_tool_options.set_visible(false);
}


void way_edit_frame_t::update_tool_options(const infra_entry_t *e)
{
	opt_way = NULL;
	opt_bridge = NULL;
	opt_tunnel = NULL;
	opt_road = false;

	tool_t *tool = e ? get_options_tool(*e) : NULL;
	waytype_t wt = invalid_wt;
	bool flat_way = false;
	if(  tool  ) {
		switch(  e->kind  ) {
			case KIND_GROUND:
			case KIND_ELEVATED: {
				opt_way = dynamic_cast<tool_build_way_t *>(tool);
				const way_desc_t *desc = static_cast<const way_desc_t *>(e->desc);
				wt = desc->is_tram() ? tram_wt : desc->get_wtyp();
				// same condition as for the dialog of the toolbar
				flat_way = desc->get_styp() == type_flat  ||  desc->get_styp() == type_tram;
				break;
			}
			case KIND_BRIDGE:
				opt_bridge = dynamic_cast<tool_build_bridge_t *>(tool);
				wt = static_cast<const bridge_desc_t *>(e->desc)->get_waytype();
				break;
			case KIND_TUNNEL:
				opt_tunnel = dynamic_cast<tool_build_tunnel_t *>(tool);
				wt = static_cast<const tunnel_desc_t *>(e->desc)->get_waytype();
				break;
		}
	}

	if(  opt_way == NULL  &&  opt_bridge == NULL  &&  opt_tunnel == NULL  ) {
		cont_tool_options.set_visible(false);
		reset_min_windowsize();
		return;
	}
	cont_tool_options.set_visible(true);

	// current settings of the tool
	overtaking_mode_t mode;
	uint8 street_flag;
	sint8 vehicle_offset;
	bool vehicle_offset_mode;
	if(  opt_way  ) {
		mode = opt_way->get_overtaking_mode();
		street_flag = opt_way->get_street_flag();
		vehicle_offset = opt_way->get_vehicle_offset();
		vehicle_offset_mode = opt_way->get_vehicle_offset_mode();
	}
	else if(  opt_bridge  ) {
		mode = opt_bridge->get_overtaking_mode();
		street_flag = opt_bridge->get_street_flag();
		vehicle_offset = opt_bridge->get_vehicle_offset();
		vehicle_offset_mode = opt_bridge->get_vehicle_offset_mode();
	}
	else {
		mode = opt_tunnel->get_overtaking_mode();
		street_flag = opt_tunnel->get_street_flag();
		vehicle_offset = opt_tunnel->get_vehicle_offset();
		vehicle_offset_mode = opt_tunnel->get_vehicle_offset_mode();
	}

	// road settings
	opt_road = wt == road_wt;
	cont_modes.set_visible( opt_road );
	cont_road_flags.set_visible( opt_road );
	for(  int i = 0;  i < 8;  i++  ) {
		bt_mode[i].pressed = overtaking_modes[i] == mode;
	}
	bt_avoid_cityroad.set_visible( opt_way != NULL  &&  flat_way );
	bt_avoid_cityroad.pressed = bt_avoid_cityroad.is_visible()  &&  (street_flag & strasse_t::AVOID_CITYROAD);
	bt_citycar_no_entry.pressed = street_flag & strasse_t::CITYCAR_NO_ENTRY;
	bt_no_building.pressed = street_flag & strasse_t::NO_BUILDING;
	bt_allow_branching_cityroad.pressed = street_flag & strasse_t::ALLOW_BRANCH_CITYROAD;
	bt_pedestrian_no_entry.pressed = street_flag & strasse_t::PEDESTRIAN_NO_ENTRY;
	// only meaningful when the road avoids becoming a city road
	bt_no_building.enable( bt_avoid_cityroad.pressed );
	bt_allow_branching_cityroad.enable( bt_avoid_cityroad.pressed );

	// height offset only for elevated ways
	cont_height_offset.set_visible( opt_way != NULL  &&  !flat_way );
	if(  opt_way  ) {
		ni_height_offset.set_value( opt_way->get_height_offset() );
	}

	bt_allow_closed_diagonal.set_visible( opt_way != NULL  ||  opt_tunnel != NULL );
	bt_allow_closed_diagonal.pressed = opt_way ? opt_way->get_allow_closed_diagonal() : opt_tunnel ? opt_tunnel->get_allow_closed_diagonal() : false;

	ni_vehicle_offset.set_value( vehicle_offset );
	bt_vehicle_offset_mode.pressed = vehicle_offset_mode;

	reset_min_windowsize();
}


void way_edit_frame_t::apply_road_options()
{
	if(  !opt_road  ) {
		return;
	}
	overtaking_mode_t mode = twoway_mode;
	for(  int i = 0;  i < 8;  i++  ) {
		if(  bt_mode[i].pressed  ) {
			mode = overtaking_modes[i];
		}
	}
	uint8 flag = 0;
	if(  bt_avoid_cityroad.is_visible()  &&  bt_avoid_cityroad.pressed  ) { flag |= strasse_t::AVOID_CITYROAD; }
	if(  bt_citycar_no_entry.pressed  )         { flag |= strasse_t::CITYCAR_NO_ENTRY; }
	if(  bt_allow_branching_cityroad.pressed  ) { flag |= strasse_t::ALLOW_BRANCH_CITYROAD; }
	if(  bt_no_building.pressed  )              { flag |= strasse_t::NO_BUILDING; }
	if(  bt_pedestrian_no_entry.pressed  )      { flag |= strasse_t::PEDESTRIAN_NO_ENTRY; }

	if(  opt_way  ) {
		opt_way->set_overtaking_mode( mode );
		opt_way->set_street_flag( flag );
	}
	else if(  opt_bridge  ) {
		opt_bridge->set_overtaking_mode( mode );
		opt_bridge->set_street_flag( flag );
	}
	else if(  opt_tunnel  ) {
		opt_tunnel->set_overtaking_mode( mode );
		opt_tunnel->set_street_flag( flag );
	}
}


bool way_edit_frame_t::action_triggered( gui_action_creator_t *comp, value_t v )
{
	for(  int i = 0;  i < 8;  i++  ) {
		if(  comp == &bt_mode[i]  ) {
			for(  int j = 0;  j < 8;  j++  ) {
				bt_mode[j].pressed = i == j;
			}
			apply_road_options();
			return true;
		}
	}
	if(  comp == &bt_avoid_cityroad  ) {
		bt_avoid_cityroad.pressed ^= 1;
		// only meaningful when the road avoids becoming a city road
		bt_no_building.enable( bt_avoid_cityroad.pressed );
		bt_allow_branching_cityroad.enable( bt_avoid_cityroad.pressed );
		if(  !bt_avoid_cityroad.pressed  ) {
			bt_no_building.pressed = false;
			bt_allow_branching_cityroad.pressed = false;
		}
		apply_road_options();
		return true;
	}
	button_t *const flags[4] = { &bt_citycar_no_entry, &bt_no_building, &bt_allow_branching_cityroad, &bt_pedestrian_no_entry };
	for(  int i = 0;  i < 4;  i++  ) {
		if(  comp == flags[i]  ) {
			flags[i]->pressed ^= 1;
			apply_road_options();
			return true;
		}
	}
	if(  comp == &bt_allow_closed_diagonal  ) {
		bt_allow_closed_diagonal.pressed ^= 1;
		if(  opt_way  ) {
			opt_way->set_allow_closed_diagonal( bt_allow_closed_diagonal.pressed );
		}
		else if(  opt_tunnel  ) {
			opt_tunnel->set_allow_closed_diagonal( bt_allow_closed_diagonal.pressed );
		}
		return true;
	}
	if(  comp == &ni_height_offset  ) {
		if(  opt_way  ) {
			opt_way->set_height_offset( (sint8)ni_height_offset.get_value() );
		}
		return true;
	}
	if(  comp == &ni_vehicle_offset  ) {
		const sint8 offset = (sint8)ni_vehicle_offset.get_value();
		if(  opt_way  ) {
			opt_way->set_vehicle_offset( offset );
		}
		else if(  opt_bridge  ) {
			opt_bridge->set_vehicle_offset( offset );
		}
		else if(  opt_tunnel  ) {
			opt_tunnel->set_vehicle_offset( offset );
		}
		return true;
	}
	if(  comp == &bt_vehicle_offset_mode  ) {
		bt_vehicle_offset_mode.pressed ^= 1;
		if(  opt_way  ) {
			opt_way->set_vehicle_offset_mode( bt_vehicle_offset_mode.pressed );
		}
		else if(  opt_bridge  ) {
			opt_bridge->set_vehicle_offset_mode( bt_vehicle_offset_mode.pressed );
		}
		else if(  opt_tunnel  ) {
			opt_tunnel->set_vehicle_offset_mode( bt_vehicle_offset_mode.pressed );
		}
		return true;
	}
	return infra_edit_frame_t::action_triggered( comp, v );
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
