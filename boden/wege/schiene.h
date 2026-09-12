/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef BODEN_WEGE_SCHIENE_H
#define BODEN_WEGE_SCHIENE_H


#include "weg.h"
#include "../../convoihandle_t.h"
#include "../../dataobj/ribi.h"

class vehicle_t;

/**
 * Class for Rails in Simutrans.
 * Trains can run over rails.
 * Every rail belongs to a section block
 */
class schiene_t : public weg_t
{
protected:
	convoihandle_t reserved;
	ribi_t::ribi   reserved_dir  = ribi_t::none;
	// the real heading (single ribi) of the primary reservation, ribi_t::none if unknown.
	// The corner_set in reserved_dir cannot tell the two opposite headings apart
	// (east-bound and west-bound both give eastwest), so it is kept separately.
	ribi_t::ribi   reserved_travel_dir = ribi_t::none;
	convoihandle_t reserved2;
	ribi_t::ribi   reserved2_dir = ribi_t::none;
	ribi_t::ribi   reserved2_travel_dir = ribi_t::none;

	// Two bends that share no ribi bits don't cross visually (NW+SE or NE+SW).
	static bool can_co_reserve_dirs(ribi_t::ribi d1, ribi_t::ribi d2) {
		return  ribi_t::are_disjoint_bends(d1, d2);
	}

public:
	/**
	 * True when a convoy heading @p travel_dir may share this tile with the convoy
	 * holding the primary reservation because the way has a vehicle offset.
	 *
	 * With offset mode 0 vehicle_t::vehicle_offset_defined_by_way() distinguishes all
	 * eight directions, so two opposite headings are drawn on opposite sides of the
	 * tile and never touch.  With offset mode 1 the direction is folded (d%4) and both
	 * convoys would get the *same* offset, so co-reservation is not allowed there.
	 *
	 * This needs the true heading, not the corner_set: only a pair of exactly opposite
	 * single directions is safe, and only on a plain through tile (no switch/crossing).
	 */
	bool can_co_reserve_offset(ribi_t::ribi travel_dir) const {
		return  reserved.is_bound()
		    &&  !reserved2.is_bound()
		    &&  ribi_t::is_single(travel_dir)
		    &&  ribi_t::is_single(reserved_travel_dir)
		    &&  travel_dir == ribi_t::backward(reserved_travel_dir)
		    &&  get_vehicle_offset_mode() == 0
		    &&  get_vehicle_offset() != 0
		    &&  ribi_t::is_twoway(get_ribi_unmasked());
	}

public:
	static const way_desc_t *default_schiene;

	static bool show_reservations;

	/**
	* File loading constructor.
	*/
	schiene_t(loadsave_t *file);

	schiene_t();

	waytype_t get_waytype() const OVERRIDE {return track_wt;}

	/**
	* @param[out] buf additional info is reservation!
	*/
	void info(cbuffer_t & buf) const OVERRIDE;

	/**
	* true, if this rail can be reserved
	*/
	bool can_reserve(convoihandle_t c, ribi_t::ribi dir = ribi_t::none, ribi_t::ribi travel_dir = ribi_t::none) const {
		if(  !reserved.is_bound()  ||  c==reserved  ||  c==reserved2  ) {
			return true;
		}
		if(  dir!=ribi_t::none  &&  get_ribi_unmasked()==ribi_t::all  &&  !reserved2.is_bound()  &&  can_co_reserve_dirs(reserved_dir, dir)  ) {
			return true;
		}
		return can_co_reserve_offset(travel_dir);
	}

	/**
	* true, if this rail can be reserved
	*/
	bool is_reserved() const { return reserved.is_bound(); }

	/**
	* true, then this rail was reserved
	*/
	bool reserve(convoihandle_t const cnv, ribi_t::ribi dir, ribi_t::ribi travel_dir = ribi_t::none);

	/**
	* releases previous reservation
	*/
	virtual bool unreserve( convoihandle_t c);

	/**
	* releases previous reservation — derives convoy handle from the vehicle
	* so that co-reserved convoys are correctly identified.
	*/
	bool unreserve( vehicle_t *v);

	/* called before deletion;
	 * last chance to unreserve tiles ...
	 */
	void cleanup(player_t *player) OVERRIDE;

	/**
	* gets the related convoi (primary slot)
	*/
	convoihandle_t get_reserved_convoi() const {return reserved;}

	/**
	* gets the direction corner_set of the primary reservation
	*/
	ribi_t::ribi get_reserved_dir() const { return reserved_dir; }

	/**
	 * True if a convoy transiting this tile with corner_set @p dir can
	 * co-reserve alongside the existing primary reservation.
	 */
	bool can_co_reserve_with(ribi_t::ribi dir, ribi_t::ribi travel_dir = ribi_t::none) const {
		return (!reserved2.is_bound() && can_co_reserve_dirs(reserved_dir, dir))
		    || can_co_reserve_offset(travel_dir);
	}

	/**
	 * Weak entry-only gate used by check_next_tile during choose-area routing.
	 * Returns true when a convoy approaching via border @p entry *might* be
	 * able to co-reserve this tile via a non-conflicting opposite bend.
	 * check_transit_tile validates the exact exit direction afterwards.
	 */
	bool can_co_reserve_approach(ribi_t::ribi entry) const {
		if (!reserved.is_bound() || reserved2.is_bound()) return false;
		if (!ribi_t::is_bend(reserved_dir)) return false;
		return (ribi_t::backward(reserved_dir) & entry) != 0;
	}

	/**
	* true if convoy c holds either the primary or the secondary reservation.
	* Use this instead of get_reserved_convoi()==c when the convoy may be
	* co-reserved (e.g. in is_next_tile_already_reserved).
	*/
	bool is_reserved_by(convoihandle_t c) const {
		return (reserved.is_bound()  &&  reserved  == c)
		    || (reserved2.is_bound() &&  reserved2 == c);
	}

	void rdwr(loadsave_t *file) OVERRIDE;

	/**
	 * if a function return here a value with TRANSPARENT_FLAGS set
	 * then a transparent outline with the color form the lower 8 Bit is drawn
	 */
	FLAGGED_PIXVAL get_outline_colour() const OVERRIDE;

	/*
	 * to show reservations if needed
	 */
	image_id get_outline_image() const OVERRIDE { return weg_t::get_image(); }
};


template<> inline schiene_t* obj_cast<schiene_t>(obj_t* const d)
{
	return dynamic_cast<schiene_t*>(d);
}

#endif
