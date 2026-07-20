/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../../simconvoi.h"
#include "../../simworld.h"
#include "../../vehicle/simvehicle.h"

#include "../../dataobj/loadsave.h"
#include "../../dataobj/translator.h"

#include "../../utils/cbuffer_t.h"

#include "../../descriptor/way_desc.h"
#include "../../bauer/wegbauer.h"

#include "schiene.h"

const way_desc_t *schiene_t::default_schiene=NULL;
bool schiene_t::show_reservations = false;


schiene_t::schiene_t() : weg_t()
{
	reserved      = convoihandle_t();
	reserved_dir  = ribi_t::none;
	reserved2     = convoihandle_t();
	reserved2_dir = ribi_t::none;

	if (schiene_t::default_schiene) {
		set_desc(schiene_t::default_schiene);
	}
	else {
		dbg->fatal("schiene_t::schiene_t()", "No rail way available!");
	}
}


schiene_t::schiene_t(loadsave_t *file) : weg_t()
{
	reserved      = convoihandle_t();
	reserved_dir  = ribi_t::none;
	reserved2     = convoihandle_t();
	reserved2_dir = ribi_t::none;
	rdwr(file);
}


void schiene_t::cleanup(player_t *)
{
	// removes reservation
	if(reserved.is_bound()) {
		set_ribi(ribi_t::none);
		reserved->suche_neue_route();
	}
	if(reserved2.is_bound()) {
		reserved2->suche_neue_route();
	}
}


void schiene_t::info(cbuffer_t & buf) const
{
	weg_t::info(buf);

	if(reserved.is_bound()) {
		const char* reserve_text = translator::translate("\nis reserved by:");
		// ignore linebreak
		if (reserve_text[0] == '\n') {
			reserve_text++;
		}
		buf.append(reserve_text);
		buf.append(reserved->get_name());
		buf.append("\n");
#ifdef DEBUG_PBS
		reserved->open_info_window();
#endif
	}
	if(reserved2.is_bound()) {
		const char* reserve_text = translator::translate("\nis co-reserved by:");
		if (reserve_text[0] == '\n') {
			reserve_text++;
		}
		buf.append(reserve_text);
		buf.append(reserved2->get_name());
		buf.append("\n");
	}
}


/**
 * true, if this rail can be reserved
 */
bool schiene_t::reserve(convoihandle_t c, ribi_t::ribi dir)
{
	if(c == reserved) {
		// Allow direction restoration when reserved_dir is still none (e.g. after
		// save/load before reserve_route() has run).  Do NOT overwrite a valid
		// corner_set: enter_tile passes get_direction() which may be a wrong diagonal
		// (e.g. SW=12 for a N→W turn) that would corrupt the PBS corner_set.
		if(reserved_dir == ribi_t::none  &&  dir != ribi_t::none) {
			reserved_dir = dir;
		}
		if(schiene_t::show_reservations) {
			set_flag( obj_t::dirty );
		}
		return true;
	}
	if(c == reserved2) {
		// Same: restore direction only when not yet set.
		if(reserved2_dir == ribi_t::none  &&  dir != ribi_t::none) {
			reserved2_dir = dir;
		}
		if(schiene_t::show_reservations) {
			set_flag( obj_t::dirty );
		}
		return true;
	}
	// for safety
	if(!reserved.is_bound()&&reserved2.is_bound()) {
		reserved = reserved2;
		reserved_dir = reserved2_dir;
		reserved2 = convoihandle_t();
		reserved2_dir=ribi_t::none;
	}
	if(!reserved.is_bound()) {
		// fresh reservation
		reserved     = c;
		reserved_dir = dir;
		/* for threeway and fourway switches we may need to alter graphic, if
		 * direction is a diagonal (i.e. on the switching part)
		 * and there are switching graphics
		 */
		if(  get_desc()!=NULL  &&  ribi_t::is_threeway(get_ribi_unmasked())  &&  ribi_t::is_bend(dir)  &&  get_desc()->has_switch_image()  &&  get_desc()->get_finance_waytype() != tram_wt  &&  !get_is_ex_image()  ) {
			mark_image_dirty( get_image(), 0 );
			mark_image_dirty( get_front_image(), 0 );
			// dir is now corner_set (entry_border|exit_border); SE/NW corners match the
			// same physical switch variant that NE/SW matched under the old ribi_type(prev,next) formula.
			set_images(image_switch, get_ribi_unmasked(), is_snow(), (dir==ribi_t::southeast  ||  dir==ribi_t::northwest) );
			set_flag( obj_t::dirty );
		}
		if(schiene_t::show_reservations) {
			set_flag( obj_t::dirty );
		}
		return true;
	}
	// tile is reserved by a different convoy — try co-reservation for non-crossing bends
	if(!reserved2.is_bound()  &&  get_ribi_unmasked()==ribi_t::all  &&  can_co_reserve_dirs(reserved_dir, dir)) {
		reserved2     = c;
		reserved2_dir = dir;
		if(schiene_t::show_reservations) {
			set_flag( obj_t::dirty );
		}
		return true;
	}
	return false;
}


/**
* releases previous reservation
* only true, if there was something to release
*/
bool schiene_t::unreserve(vehicle_t* v)
{
	// Derive the convoy handle from the vehicle so that co-reserved convoys
	// are correctly matched.  The old inline always passed 'reserved' which
	// silently cleared the primary slot even when the vehicle belonged to
	// the secondary convoy.
	convoi_t* c = v ? v->get_convoi() : nullptr;
	return unreserve(c ? c->self : convoihandle_t());
}


bool schiene_t::unreserve(convoihandle_t c)
{
	if(reserved.is_bound()  &&  reserved == c) {
		// promote co-reservation to primary slot (if any)
		reserved      = reserved2;
		reserved_dir  = reserved2_dir;
		reserved2     = convoihandle_t();
		reserved2_dir = ribi_t::none;
		if(schiene_t::show_reservations) {
			set_flag( obj_t::dirty );
		}
		return true;
	}
	if(reserved2.is_bound()  &&  reserved2 == c) {
		reserved2     = convoihandle_t();
		reserved2_dir = ribi_t::none;
		if(schiene_t::show_reservations) {
			set_flag( obj_t::dirty );
		}
		return true;
	}
	return false;
}



void schiene_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "schiene_t" );

	weg_t::rdwr(file);

	if(file->is_version_less(99, 8)) {
		sint32 blocknr=-1;
		file->rdwr_long(blocknr);
	}

	if(file->is_version_less(89, 0)) {
		uint8 dummy;
		file->rdwr_byte(dummy);
		set_electrify(dummy);
	}

	if(file->is_saving()) {
		const char *s = get_desc()->get_name();
		file->rdwr_str(s);
	}
	else {
		char bname[128];
		file->rdwr_str(bname, lengthof(bname));

		int old_max_speed = get_max_speed();
		const way_desc_t *desc = way_builder_t::get_desc(bname);

		if(desc==NULL) {
			int old_max_speed=get_max_speed();
			desc = way_builder_t::get_desc(translator::compatibility_name(bname));
			if(desc==NULL) {
				desc = default_schiene;
				if (!desc) {
					dbg->fatal("schiene_t::rdwr", "Trying to load train tracks but pakset has none!");
				}
				welt->add_missing_paks( bname, karte_t::MISSING_WAY );
			}
			dbg->warning("schiene_t::rdwr()", "Unknown rail '%s' replaced by '%s' (old_max_speed %i)", bname, desc->get_name(), old_max_speed );
		}

		set_desc(desc);
		if(old_max_speed>0) {
			set_max_speed(old_max_speed);
		}
//		DBG_MESSAGE("schiene_t::rdwr","track %s at (%s) max_speed %i", bname, get_pos().get_str(), old_max_speed);
	}
}


FLAGGED_PIXVAL schiene_t::get_outline_colour() const
{
	if (!show_reservations || !reserved.is_bound()) {
		return 0;
	}

	return TRANSPARENT75_FLAG | OUTLINE_FLAG | color_idx_to_rgb(COL_RED);
}
