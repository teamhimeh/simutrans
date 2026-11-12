/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include <algorithm>

#include "../simdebug.h"
#include "../simworld.h"
#include "simobj.h"
#include "../boden/wege/schiene.h"
#include "../boden/grund.h"
#include "../display/simimg.h"
#include "../dataobj/ribi.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/environment.h"
#include "../utils/cbuffer_t.h"

#include "signal.h"


signal_t::signal_t(loadsave_t *file) :
	roadsign_t(file)
{
	if(desc==NULL) {
		desc = roadsign_t::default_signal;
	}
	state = STATE_RED;
}


void signal_t::info(cbuffer_t & buf) const
{
	// well, needs to be done
	obj_t::info(buf);

	buf.printf("%s\n%s%u", translator::translate(desc->get_name()), translator::translate("\ndirection:"), get_dir());

	if (char const* const maker = desc->get_copyright()) {
		buf.append("\n\n");
		buf.printf(translator::translate("Constructed by %s"), maker);
	}
}


void signal_t::calc_image()
{
	foreground_image = IMG_EMPTY;
	image_id image = IMG_EMPTY;
	uint16 diag[4] = { roadsign_desc_t::NO_DIAGONAL, roadsign_desc_t::NO_DIAGONAL, roadsign_desc_t::NO_DIAGONAL, roadsign_desc_t::NO_DIAGONAL };
	// true when the pak supplied an explicit front image for this signal
	bool explicit_front = false;

	after_xoffset = 0;
	after_yoffset = 0;
	sint8 xoff = 0, yoff = 0;
	const bool left_swap = welt->get_settings().is_signals_left()  &&  desc->get_offset_left();
	// use the drive-on-left variant lists when the pak has them and signals
	// stand on the left; the lists repeat the layout of the normal one, so
	// the state and electrification arithmetic below applies unchanged
	const bool left_variant = welt->get_settings().is_signals_left()  &&  desc->has_left_images();

	grund_t *gr = welt->lookup(get_pos());
	if(gr) {

		const slope_t::type full_hang = gr->get_weg_hang();
		const sint8 hang_diff = slope_t::max_diff(full_hang);
		const ribi_t::ribi hang_dir = ribi_t::backward( ribi_type(full_hang) );

		set_flag(obj_t::dirty);

		weg_t *sch = gr->get_weg(desc->get_wtyp()!=tram_wt ? desc->get_wtyp() : track_wt);
		if(sch) {
			uint16 offset=0;
			ribi_t::ribi dir = sch->get_ribi_unmasked() & (~calc_mask());
			if(sch->is_electrified()  &&  (desc->get_count()/8)>1) {
				offset = (desc->is_pre_signal()  ||  desc->is_priority_signal()) ? 12 : 8;
			}

			// the bend comes from the unmasked ribi: a one way signal masks a direction away,
			// and a single direction is not a bend. The diagonal rows are twice as long
			if(  desc->has_diagonal_image()  ) {
				const weg_t *w = get_weg_here();
				if(  w != NULL  &&  w->calc_diagonal()  ) {
					calc_diagonal_index( diag, w->get_ribi_unmasked(), state*8 + offset*2 );
				}
			}

			// vertical offset of the signal positions
			if(full_hang==slope_t::flat) {
				yoff = -gr->get_weg_yoff();
				after_yoffset = yoff;
			}
			else {
				const ribi_t::ribi test_hang = left_swap ? ribi_t::backward(hang_dir) : hang_dir;
				if(test_hang==ribi_t::east ||  test_hang==ribi_t::north) {
					yoff = -TILE_HEIGHT_STEP*hang_diff;
					after_yoffset = 0;
				}
				else {
					yoff = 0;
					after_yoffset = -TILE_HEIGHT_STEP*hang_diff;
				}
			}

			// and now calculate the images:
			// we need to hide the "second" image on tunnel entries
			ribi_t::ribi temp_dir = dir;
			if(  gr->get_typ()==grund_t::tunnelboden  &&  gr->ist_karten_boden()  &&
				(grund_t::underground_mode==grund_t::ugm_none  ||  (grund_t::underground_mode==grund_t::ugm_level  &&  gr->get_hoehe()<grund_t::underground_level))   ) {
				// entering tunnel here: hide the image further in if not undergroud/sliced
				const ribi_t::ribi tunnel_hang_dir = ribi_t::backward( ribi_type(gr->get_grund_hang()) );
				if(  tunnel_hang_dir==ribi_t::east ||  tunnel_hang_dir==ribi_t::north  ) {
					temp_dir &= ~ribi_t::southwest;
				}
				else {
					temp_dir &= ~ribi_t::northeast;
				}
			}

			// signs for left side need other offsets and other front/back order
			if(  left_swap  ) {
				const sint16 XOFF = 2*desc->get_offset_left();
				const sint16 YOFF = desc->get_offset_left();

				if(temp_dir&ribi_t::east) {
					image = desc->get_image_id(3+state*4+offset, diag[3], left_variant);
					xoff += XOFF;
					yoff += -YOFF;
				}

				if(temp_dir&ribi_t::north) {
					if(image!=IMG_EMPTY) {
						foreground_image = desc->get_image_id(0+state*4+offset, diag[0], left_variant);
						after_xoffset += -XOFF;
						after_yoffset += -YOFF;
					}
					else {
						image = desc->get_image_id(0+state*4+offset, diag[0], left_variant);
						xoff += -XOFF;
						yoff += -YOFF;
					}
				}

				if(temp_dir&ribi_t::west) {
					foreground_image = desc->get_image_id(2+state*4+offset, diag[2], left_variant);
					after_xoffset += -XOFF;
					after_yoffset += YOFF;
				}

				if(temp_dir&ribi_t::south) {
					if(foreground_image!=IMG_EMPTY) {
						image = desc->get_image_id(1+state*4+offset, diag[1], left_variant);
						xoff += XOFF;
						yoff += YOFF;
					}
					else {
						foreground_image = desc->get_image_id(1+state*4+offset, diag[1], left_variant);
						after_xoffset += XOFF;
						after_yoffset += YOFF;
					}
				}
			}
			else {
				if(temp_dir&ribi_t::east) {
					foreground_image = desc->get_image_id(3+state*4+offset, diag[3], left_variant);
				}

				if(temp_dir&ribi_t::north) {
					if(foreground_image==IMG_EMPTY) {
						foreground_image = desc->get_image_id(0+state*4+offset, diag[0], left_variant);
					}
					else {
						image = desc->get_image_id(0+state*4+offset, diag[0], left_variant);
					}
				}

				if(temp_dir&ribi_t::west) {
					image = desc->get_image_id(2+state*4+offset, diag[2], left_variant);
				}

				if(temp_dir&ribi_t::south) {
					if(image==IMG_EMPTY) {
						image = desc->get_image_id(1+state*4+offset, diag[1], left_variant);
					}
					else {
						foreground_image = desc->get_image_id(1+state*4+offset, diag[1], left_variant);
					}
				}
			}

			// a pak that defines front images wants image[] drawn behind the
			// vehicles and frontimage[] before them, instead of the position
			// heuristics above. Only a signal showing a single direction has
			// both slots free for that; two-direction signals keep the pairing above
			if(  ribi_t::is_single(temp_dir)  &&  (desc->get_flags()&roadsign_desc_t::ONLY_BACKIMAGE)==0  ) {
				const uint8 i = single_dir_index(temp_dir);
				const image_id front = desc->get_front_image_id( i+state*4+offset, diag[i], left_variant );
				if(  front != IMG_EMPTY  ) {
					// back and front are two parts of one signal standing in one place
					if(  image != IMG_EMPTY  ) {
						after_xoffset = xoff;
						after_yoffset = yoff;
					}
					else {
						xoff = after_xoffset;
						yoff = after_yoffset;
					}
					image            = desc->get_image_id( i+state*4+offset, diag[i], left_variant );
					foreground_image = front;
					explicit_front   = true;
				}
			}
		}
	}
	// swap front/back if the pak requires it (not needed when it said explicitly which is which)
	if(  !explicit_front  &&  welt->get_settings().get_signal_reverse_front_back() && (desc->get_flags()&roadsign_desc_t::ONLY_BACKIMAGE)==0  ) {
		std::swap( image, foreground_image );
		std::swap( xoff, after_xoffset );
		std::swap( yoff, after_yoffset );
	}
	// only background image used.
	if(  desc->get_flags()&roadsign_desc_t::ONLY_BACKIMAGE  ) {
		if(foreground_image!=IMG_EMPTY) {
			image = foreground_image;
		}
		foreground_image = IMG_EMPTY;
	}
	set_xoff( xoff );
	set_yoff( yoff );
	set_image(image);
}
