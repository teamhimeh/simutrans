/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../simworld.h"
#include "../simmenu.h"

#include "../obj/roadsign.h"

#include "../descriptor/roadsign_desc.h"
#include "../descriptor/skin_desc.h"

#include "../dataobj/translator.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/environment.h"

#include "../utils/cbuffer_t.h"

#include "roadsign_edit.h"


static bool is_traffic_light(const roadsign_desc_t *desc)
{
	return !desc->is_signal_type()  &&  !desc->is_private_way()  &&  desc->get_count() > 4;
}


roadsign_edit_frame_t::roadsign_edit_frame_t(player_t* player_, waytype_t wt) :
	infra_edit_frame_t("roadsign builder", player_, wt)
{
	add_filter_button( bt_signal,    "is_signal",    false );
	add_filter_button( bt_no_signal, "is_no_signal", false );

	tbl_filter->new_component<gui_label_t>("Type");
	tbl_filter->add_component( &cb_type );
	cb_type.add_listener(this);
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("All"),              IMG_EMPTY, IMG_EMPTY, (sint32)TYPE_ALL );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Signal"),           IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::SIGN_SIGNAL );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Pre-signal"),       IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::SIGN_PRE_SIGNAL );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Priority signal"),  IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::SIGN_PRIORITY_SIGNAL );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Longblock signal"), IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::SIGN_LONGBLOCK_SIGNAL );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Choose sign"),      IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::CHOOSE_SIGN );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("End of choose area"), IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::END_OF_CHOOSE_AREA );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("One-way sign"),     IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::ONE_WAY );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Private road"),     IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::PRIVATE_ROAD );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("No foreground"),    IMG_EMPTY, IMG_EMPTY, (sint32)roadsign_desc_t::ONLY_BACKIMAGE );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Traffic light"),    IMG_EMPTY, IMG_EMPTY, (sint32)TYPE_TRAFFIC_LIGHT );
	cb_type.new_component<gui_image_choice_item_t>( translator::translate("Plain sign"),       IMG_EMPTY, IMG_EMPTY, (sint32)TYPE_PLAIN_SIGN );
	cb_type.set_selection(0);

	add_filter_number( ni_min_cost, "Min. cost", 0, 9999999, 1 );

	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_MAINTENANCE);
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_SPEED);
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_TYPE);
	set_sortedby(gui_sorting_item_t::BY_TYPE);

	fill_list();
	reset_min_windowsize();
}


sint32 roadsign_edit_frame_t::get_type_filter() const
{
	if(  const gui_image_choice_item_t *item = dynamic_cast<const gui_image_choice_item_t *>( cb_type.get_selected_item() )  ) {
		return item->value;
	}
	return TYPE_ALL;
}


void roadsign_edit_frame_t::collect_entries()
{
	const sint32 type_filter = get_type_filter();
	const sint64 min_cost = (sint64)ni_min_cost.get_value() * 100;

	FOR(stringhashtable_tpl<roadsign_desc_t const*>, const& i, roadsign_t::get_desc_table()) {
		roadsign_desc_t const* const desc = i.value;
		if(  desc->get_builder() == NULL  ||  !is_waytype_ok( desc->get_wtyp() )  ) {
			continue;
		}
		if(  (bt_signal.pressed  &&  !desc->is_signal_type())  ||  (bt_no_signal.pressed  &&  desc->is_signal_type())  ) {
			continue;
		}
		switch(  type_filter  ) {
			case TYPE_ALL:
				break;
			case TYPE_TRAFFIC_LIGHT:
				if(  !is_traffic_light(desc)  ) {
					continue;
				}
				break;
			case TYPE_PLAIN_SIGN:
				if(  desc->get_flags() != roadsign_desc_t::NONE  ||  is_traffic_light(desc)  ) {
					continue;
				}
				break;
			default:
				if(  (desc->get_flags() & type_filter) == 0  ) {
					continue;
				}
		}
		if(  desc->get_price() < min_cost  ||  !is_timeline_ok(desc)  ||  !is_name_ok(desc->get_name())  ) {
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
		e.speed       = desc->get_min_speed();
		// sort by type: plain signs, other signs, traffic lights, then signals
		e.kind        = (uint16)(is_traffic_light(desc) ? 0x4000 : desc->is_signal_type() ? 0x8000 | desc->get_flags() : desc->get_flags());
		e.intro_date  = desc->get_intro_year_month();
		e.retire_date = desc->get_retire_year_month();
		e.color       = desc->is_signal_type() ? color_idx_to_rgb(COL_DARK_BLUE + env_t::gui_player_color_dark) : SYSCOL_TEXT;
		entries.append( e );
	}
}


/// image names in the image[direction][state] syntax of normal signs and signals
static void roadsign_image_name(char *buf, uint16 index)
{
	static const char* const dirs[4] = { "n", "s", "e", "w" };
	sprintf( buf, "image[%s][%d]", dirs[index % 4], index / 4 );
}


void roadsign_edit_frame_t::fill_image_choices(const infra_entry_t &e)
{
	const roadsign_desc_t *desc = static_cast<const roadsign_desc_t *>(e.desc);
	char name[64];
	for(  uint16 i = 0;  i < desc->get_count();  i++  ) {
		roadsign_image_name( name, i );
		add_image_choice( name, desc->get_image_id( (ribi_t::dir)i ), desc->get_front_image_id( i, roadsign_desc_t::NO_DIAGONAL, false ) );
	}
}


void roadsign_edit_frame_t::fill_all_image_names()
{
	// as many images as the sign with most images has
	uint16 max_count = 4;
	FOR(stringhashtable_tpl<roadsign_desc_t const*>, const& i, roadsign_t::get_desc_table()) {
		max_count = (uint16)max(max_count, i.value->get_count());
	}
	char name[64];
	for(  uint16 i = 0;  i < max_count;  i++  ) {
		roadsign_image_name( name, i );
		add_image_choice( name, IMG_EMPTY, IMG_EMPTY );
	}
}


void roadsign_edit_frame_t::append_info(const infra_entry_t &e)
{
	const roadsign_desc_t *desc = static_cast<const roadsign_desc_t *>(e.desc);
	if(  desc->is_signal_type()  ) {
		buf.printf( "%s\n", translator::translate("Signal") );
	}
	if(  desc->is_pre_signal()  ) {
		buf.printf( " - %s\n", translator::translate("Pre-signal") );
	}
	if(  desc->is_priority_signal()  ) {
		buf.printf( " - %s\n", translator::translate("Priority signal") );
	}
	if(  desc->is_longblock_signal()  ) {
		buf.printf( " - %s\n", translator::translate("Longblock signal") );
	}
	if(  desc->is_choose_sign()  ) {
		buf.printf( " - %s\n", translator::translate("Choose sign") );
	}
	if(  desc->is_end_choose_signal()  ) {
		buf.printf( " - %s\n", translator::translate("End of choose area") );
	}
	if(  desc->is_single_way()  ) {
		buf.printf( " - %s\n", translator::translate("One-way sign") );
	}
	if(  desc->is_private_way()  ) {
		buf.printf( " - %s\n", translator::translate("Private road") );
	}
	if(  is_traffic_light(desc)  ) {
		buf.printf( " - %s\n", translator::translate("Traffic light") );
	}
	if(  desc->get_min_speed() > 0  ) {
		buf.printf( "%s: %d km/h\n", translator::translate("Min. speed"), desc->get_min_speed() );
	}
}


bool roadsign_edit_frame_t::action_triggered( gui_action_creator_t *comp, value_t e )
{
	// "only signals" and "no signals" exclude each other
	if(  comp == &bt_signal  ) {
		bt_signal.pressed ^= 1;
		if(  bt_signal.pressed  ) {
			bt_no_signal.pressed = false;
		}
		fill_list();
		return true;
	}
	if(  comp == &bt_no_signal  ) {
		bt_no_signal.pressed ^= 1;
		if(  bt_no_signal.pressed  ) {
			bt_signal.pressed = false;
		}
		fill_list();
		return true;
	}
	if(  comp == &cb_type  ) {
		fill_list();
		return true;
	}
	return infra_edit_frame_t::action_triggered( comp, e );
}


void roadsign_edit_frame_t::rdwr( loadsave_t *file )
{
	uint8 type = (uint8)cb_type.get_selection();
	file->rdwr_byte( type );
	if(  file->is_loading()  &&  type < cb_type.count_elements()  ) {
		cb_type.set_selection( type );
	}
	infra_edit_frame_t::rdwr( file );
}
