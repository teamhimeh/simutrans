/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../simworld.h"
#include "../simmenu.h"
#include "../simhalt.h"

#include "../bauer/hausbauer.h"

#include "../descriptor/building_desc.h"
#include "../descriptor/skin_desc.h"

#include "../dataobj/translator.h"
#include "../dataobj/environment.h"

#include "../player/simplay.h"

#include "../utils/cbuffer_t.h"

#include "halt_edit.h"


halt_edit_frame_t::halt_edit_frame_t(player_t* player_, waytype_t wt) :
	infra_edit_frame_t("halt builder", player_, wt)
{
	add_filter_button( bt_stop,      "stop",      true );
	add_filter_button( bt_extension, "extension", true );
	add_filter_button( bt_harbour,   "harbour",   true );
	// keep the place in the grid when hidden (only shown for ships)
	bt_harbour.set_rigid(true);
	tbl_filter->new_component<gui_empty_t>();

	add_filter_button( bt_pax,  "Passagiere", true );
	add_filter_button( bt_mail, "Post",       true );
	add_filter_button( bt_ware, "Fracht",     true );
	tbl_filter->new_component<gui_empty_t>();

	add_filter_button( bt_dims_end,      "for end",        false );
	add_filter_button( bt_dims_many,     "for many tiles", false );
	add_filter_button( bt_dims_diagonal, "for diagonal",   false );
	add_filter_button( bt_dims_slope,    "for slope",      false );
	bt_dims_end.set_tooltip( translator::translate("4 layouts") );
	bt_dims_many.set_tooltip( translator::translate("more than 15 layouts") );
	bt_dims_diagonal.set_tooltip( translator::translate("more than 47 layouts") );
	bt_dims_slope.set_tooltip( translator::translate("more than 79 layouts") );

	add_filter_number( ni_min_level, "Min. level", 0, 65535 );
	add_filter_number( ni_max_level, "Max. level", 0, 65535, 1, "0 = no limit" );
	add_filter_number( ni_max_cost,  "Max. cost",  0, 9999999, 1, "0 = no limit" );

	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_LEVEL);
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_MAINTENANCE);
	set_sortedby(gui_sorting_item_t::BY_LEVEL);

	update_layout_filters();

	fill_list();
	reset_min_windowsize();
}


void halt_edit_frame_t::update_layout_filters()
{
	button_t *const dims[4] = { &bt_dims_end, &bt_dims_many, &bt_dims_diagonal, &bt_dims_slope };
	for(  int i = 0;  i < 4;  i++  ) {
		dims[i]->enable( bt_stop.pressed );
	}
	// harbours exist only for ships
	bt_harbour.set_visible( filter_waytype == water_wt  ||  filter_waytype == ignore_wt );
}


void halt_edit_frame_t::filter_changed()
{
	update_layout_filters();
	fill_list();
}


void halt_edit_frame_t::collect_entries()
{
	const vector_tpl<const building_desc_t *> *list = hausbauer_t::get_list( building_desc_t::generic_stop );
	if(  list == NULL  ) {
		return;
	}

	const uint8 goods_mask = (bt_pax.pressed ? haltestelle_t::PAX : 0) | (bt_mail.pressed ? haltestelle_t::POST : 0) | (bt_ware.pressed ? haltestelle_t::WARE : 0);
	const sint32 min_level = ni_min_level.get_value();
	const sint32 max_level = ni_max_level.get_value();

	FOR(vector_tpl<building_desc_t const*>, const desc, *list) {
		if(  desc->get_builder() == NULL  ) {
			continue;
		}

		uint8 kind;
		waytype_t wt;
		switch(  desc->get_type()  ) {
			case building_desc_t::generic_stop:      kind = KIND_STOP;      wt = (waytype_t)desc->get_extra(); break;
			case building_desc_t::generic_extension: kind = KIND_EXTENSION; wt = (waytype_t)desc->get_extra(); break;
			case building_desc_t::dock:
			case building_desc_t::flat_dock:         kind = KIND_HARBOUR;   wt = water_wt;                     break;
			default: continue; // depots, headquarters ...
		}

		if(  !is_waytype_ok(wt)  ) {
			continue;
		}
		if(  (kind == KIND_STOP  &&  !bt_stop.pressed)  ||  (kind == KIND_EXTENSION  &&  !bt_extension.pressed)  ||  (kind == KIND_HARBOUR  &&  !bt_harbour.pressed)  ) {
			continue;
		}

		// goods: show the building, if it handles any of the selected goods
		const uint8 enables = desc->get_enabled() & (haltestelle_t::PAX | haltestelle_t::POST | haltestelle_t::WARE);
		if(  enables ? (enables & goods_mask) == 0 : goods_mask != (haltestelle_t::PAX | haltestelle_t::POST | haltestelle_t::WARE)  ) {
			continue;
		}

		// layouts (only for stops)
		if(  kind == KIND_STOP  ) {
			const uint8 layouts = desc->get_all_layouts();
			if(  (bt_dims_end.pressed  &&  layouts != 4)
				||  (bt_dims_many.pressed  &&  layouts <= 15)
				||  (bt_dims_diagonal.pressed  &&  layouts <= 47)
				||  (bt_dims_slope.pressed  &&  layouts <= 79)  ) {
				continue;
			}
		}

		infra_entry_t e;
		e.desc        = desc;
		e.name        = desc->get_name();
		e.copyright   = desc->get_copyright();
		e.tool        = desc->get_builder();
		e.icon        = desc->get_cursor() ? desc->get_cursor()->get_image_id(1) : IMG_EMPTY;
		e.cursor      = desc->get_cursor() ? desc->get_cursor()->get_image_id(0) : IMG_EMPTY;
		e.price       = desc->get_price(welt);
		e.maintenance = desc->get_maintenance(welt);
		e.level       = desc->get_level();
		e.intro_date  = desc->get_intro_year_month();
		e.retire_date = desc->get_retire_year_month();
		e.kind        = kind;
		e.color       = kind == KIND_EXTENSION ? color_idx_to_rgb(COL_DARK_BLUE + env_t::gui_player_color_dark) :
		                kind == KIND_HARBOUR   ? color_idx_to_rgb(40 + env_t::gui_player_color_dark) :
		                SYSCOL_TEXT;

		if(  e.level >= min_level
			&&  (max_level <= 0  ||  e.level <= max_level)
			&&  is_below_limit( e.price, ni_max_cost.get_value() )
			&&  is_timeline_ok( desc )
			&&  is_name_ok( e.name )  ) {
			entries.append( e );
		}
	}
}


void halt_edit_frame_t::fill_image_choices(const infra_entry_t &e)
{
	const building_desc_t *desc = static_cast<const building_desc_t *>(e.desc);
	char name[64];
	for(  uint8 layout = 0;  layout < desc->get_all_layouts();  layout++  ) {
		sprintf( name, "BackImage[%d]", layout );
		add_image_choice( name, IMG_EMPTY, IMG_EMPTY, layout );
	}
}


void halt_edit_frame_t::fill_all_image_names()
{
	// as many layouts as the station with most layouts has
	uint8 max_layouts = 1;
	if(  const vector_tpl<const building_desc_t *> *list = hausbauer_t::get_list( building_desc_t::generic_stop )  ) {
		FOR(vector_tpl<building_desc_t const*>, const desc, *list) {
			max_layouts = (uint8)max(max_layouts, desc->get_all_layouts());
		}
	}
	char name[64];
	for(  uint8 layout = 0;  layout < max_layouts;  layout++  ) {
		sprintf( name, "BackImage[%d]", layout );
		add_image_choice( name, IMG_EMPTY, IMG_EMPTY, layout );
	}
}


void halt_edit_frame_t::update_preview()
{
	const infra_entry_t *e = get_selected_entry();
	const infra_image_choice_t img = e ? get_wanted_image( *e ) : infra_image_choice_t();
	if(  e  &&  img.value >= 0  ) {
		// a layout of the building (only offered, if the building has it)
		building_image.init( static_cast<const building_desc_t *>(e->desc), img.value );
		building_image.set_visible( true );
		preview.set_images( IMG_EMPTY, IMG_EMPTY, welt->get_active_player_nr() );
		preview.set_visible( false );
	}
	else {
		building_image.init( NULL, 0 );
		building_image.set_visible( false );
		preview.set_visible( true );
		infra_edit_frame_t::update_preview();
	}
}


void halt_edit_frame_t::new_thumbnail(const infra_entry_t &e, const char *name)
{
	const infra_image_choice_t img = get_wanted_image( e );
	if(  img.value >= 0  ) {
		// a layout of the building
		scl.new_component<building_thumbnail_t>( static_cast<const building_desc_t *>(e.desc), name, get_thumbnail_size(), (int)img.value );
	}
	else {
		infra_edit_frame_t::new_thumbnail( e, name );
	}
}


void halt_edit_frame_t::append_info(const infra_entry_t &e)
{
	const building_desc_t *desc = static_cast<const building_desc_t *>(e.desc);
	switch(  e.kind  ) {
		case KIND_STOP:      buf.printf( "%s\n", translator::translate("stop") );      break;
		case KIND_EXTENSION: buf.printf( "%s\n", translator::translate("extension") ); break;
		case KIND_HARBOUR:   buf.printf( "%s\n", translator::translate("harbour") );   break;
	}
	buf.printf( "%s: %d\n", translator::translate("Level"), desc->get_level() );
	buf.printf( "%s: %d\n", translator::translate("Capacity"), desc->get_capacity() );
	buf.printf( "%s: %dx%d\n", translator::translate("Size"), desc->get_size().x, desc->get_size().y );
	buf.printf( "%s: %d\n", translator::translate("Layouts"), desc->get_all_layouts() );

	const uint8 enables = desc->get_enabled();
	if(  enables & haltestelle_t::PAX  ) {
		buf.printf( " - %s\n", translator::translate("Passagiere") );
	}
	if(  enables & haltestelle_t::POST  ) {
		buf.printf( " - %s\n", translator::translate("Post") );
	}
	if(  enables & haltestelle_t::WARE  ) {
		buf.printf( " - %s\n", translator::translate("Fracht") );
	}
}
