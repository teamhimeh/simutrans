/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef OBJ_WAYOBJ_H
#define OBJ_WAYOBJ_H


#include "../simtypes.h"
#include "../display/simimg.h"
#include "simobj.h"
#include "../dataobj/ribi.h"
#include "../descriptor/way_obj_desc.h"
#include "../tpl/stringhashtable_tpl.h"

class player_t;
class karte_t;
class koord;
class grund_t;
class tool_selector_t;

/* wayobj enable various functionality of ways, most prominent are overhead power lines */
class wayobj_t : public obj_no_info_t
{
private:
	const way_obj_desc_t *desc;

	uint8 diagonal:1;
	uint8 hang:7;

	// direction of this wayobj
	uint8 nw:1;
	uint8 dir:7;

	// close diagonals on a fourway way: 0=no, 1=northwest+southeast, 2=northeast+southwest
	uint8 close_diagonal:2;

	// two same-waytype disjoint diagonal legs below, both reached by our dir, and no close
	// diagonal image for them: each leg gets its own image (leg_dir_a is drawn as our normal
	// image, leg_dir_b additionally by display()/display_after())
	uint8 two_legs:1;
	// bit 0: leg a drawn diagonal, bit 1: leg b drawn diagonal
	uint8 leg_diagonal:2;
	uint8 leg_dir_a:4;
	uint8 leg_dir_b:4;

	// image of a close diagonal
	image_id get_close_diagonal_image(bool front) const {
		return front ? desc->get_front_close_diagonal_image_id(close_diagonal-1) : desc->get_back_close_diagonal_image_id(close_diagonal-1);
	}

	// image of one of two legs
	image_id get_leg_image(ribi_t::ribi leg_dir, bool leg_diag, bool front) const {
		if(  leg_diag  ) {
			return front ? desc->get_front_diagonal_image_id(leg_dir) : desc->get_back_diagonal_image_id(leg_dir);
		}
		return front ? desc->get_front_image_id(leg_dir) : desc->get_back_image_id(leg_dir);
	}

	// draws an additional image like display_after() does
	void display_extra_image(image_id image, int xpos, int ypos  CLIP_NUM_DEF) const;

	static const uint8 dir_unknown = 127;

	ribi_t::ribi find_next_ribi(const grund_t *start, const ribi_t::ribi dir, const waytype_t wt) const;

	// true when leg_dir is a bend we have a diagonal image for
	bool is_diagonal_leg(ribi_t::ribi leg_dir) const;

	// recalculates the images of the wayobjs on the neighbouring tiles (not recursively)
	void calc_neighbour_images(const grund_t *gr, waytype_t wt) const;


public:
	wayobj_t(koord3d pos, player_t *owner, ribi_t::ribi dir, const way_obj_desc_t *desc);

	wayobj_t(loadsave_t *file);

	virtual ~wayobj_t();

	const way_obj_desc_t *get_desc() const {return desc;}

	void rotate90() OVERRIDE;

	image_id is_need_crossing_image(ribi_t::ribi ribi, bool front) const;

#if COLOUR_DEPTH != 0 && MULTI_THREAD != 0
	/**
	* the back image, drawn before vehicles
	*/
	image_id get_image() const OVERRIDE {
		return close_diagonal ? get_close_diagonal_image(false) : two_legs ? get_leg_image(leg_dir_a, leg_diagonal&1, false) : hang ? desc->get_back_slope_image_id(hang) :
			(diagonal ? desc->get_back_diagonal_image_id(dir) :
				(ribi_t::is_threeway(dir) && desc->has_switch_image() ? is_need_crossing_image(dir,false) : desc->get_back_image_id(dir))
			);
	}

	/**
	 * the front image, drawn after everything else
	 */
	image_id get_front_image() const OVERRIDE {
		return close_diagonal ? get_close_diagonal_image(true) : two_legs ? get_leg_image(leg_dir_a, leg_diagonal&1, true) : hang ? desc->get_front_slope_image_id(hang) :
			(diagonal ? desc->get_front_diagonal_image_id(dir) :
				(ribi_t::is_threeway(dir) && desc->has_switch_image() ? is_need_crossing_image(dir,true) : desc->get_front_image_id(dir))
			);
	}
#else
	/**
	* the back image, drawn before vehicles
	*/
	image_id get_image() const OVERRIDE {
		return close_diagonal ? get_close_diagonal_image(false) : two_legs ? get_leg_image(leg_dir_a, leg_diagonal&1, false) : hang ? desc->get_back_slope_image_id(hang) :
			(diagonal ? desc->get_back_diagonal_image_id(dir) : desc->get_back_image_id(dir));
	}

	/**
	 * the front image, drawn after everything else
	 */
	image_id get_front_image() const OVERRIDE {
		return close_diagonal ? get_close_diagonal_image(true) : two_legs ? get_leg_image(leg_dir_a, leg_diagonal&1, true) : hang ? desc->get_front_slope_image_id(hang) :
			(diagonal ? desc->get_front_diagonal_image_id(dir) : desc->get_front_image_id(dir));
	}
#endif
	image_id get_image_test() const {
		return two_legs ? get_leg_image(leg_dir_a, leg_diagonal&1, false) : hang ? desc->get_back_slope_image_id(hang) :
			(diagonal ? desc->get_back_diagonal_image_id(dir) : desc->get_back_image_id(dir));
	}

	image_id get_front_image_test() const {
		return two_legs ? get_leg_image(leg_dir_a, leg_diagonal&1, true) : hang ? desc->get_front_slope_image_id(hang) :
			(diagonal ? desc->get_front_diagonal_image_id(dir) : desc->get_front_image_id(dir));
	}

	/**
	 * also draws the image of the second leg (see two_legs)
	 */
	void display(int xpos, int ypos  CLIP_NUM_DEF) const OVERRIDE;
#ifdef MULTI_THREAD
	void display_after(int xpos, int ypos, const sint8 clip_num) const OVERRIDE;
#else
	void display_after(int xpos, int ypos, bool is_global) const OVERRIDE;
#endif

	typ get_typ() const OVERRIDE { return wayobj; }

	/**
	 * waytype associated with this object
	 */
	waytype_t get_waytype() const OVERRIDE { return desc ? desc->get_wtyp() : invalid_wt; }

	void calc_image() OVERRIDE;

	void rdwr(loadsave_t *file) OVERRIDE;

	void cleanup(player_t *player) OVERRIDE;

	const char* is_deletable(const player_t *player) OVERRIDE;

	void finish_rd(const uint8 loaded_OTRP_version) OVERRIDE;

	// specific for wayobj
	// also updates which of two same-waytype diagonal legs an overhead line electrifies
	void set_dir(ribi_t::ribi d);
	ribi_t::ribi get_dir() const { return dir; }

	/**
	 * (Re)electrifies the way(s) below an overhead line. A single way is electrified as long
	 * as the line exists. When two same-waytype disjoint diagonal legs share the tile, this one
	 * wayobj spans both, so each leg is electrified exactly when our dir reaches it.
	 */
	void update_electrification();

	/* the static routines */
private:
	static stringhashtable_tpl<const way_obj_desc_t *> table;

public:
	static const way_obj_desc_t *default_oberleitung;

	// use this constructor; it will extend a matching existing wayobj
	static void extend_wayobj(koord3d pos, player_t *owner, ribi_t::ribi dir, const way_obj_desc_t *desc, bool keep_existing_faster_way);

	static bool register_desc(way_obj_desc_t *desc);
	static bool successfully_loaded();

	// search an object (currently only used by AI for catenary)
	static const way_obj_desc_t *get_overhead_line(waytype_t wt, uint16 time);

	static const way_obj_desc_t *find_desc(const char *);

	/**
	 * Fill menu with icons of given stops from the list
	 */
	static void fill_menu(tool_selector_t *tool_selector, waytype_t wtyp, sint16 sound_ok);

	static const stringhashtable_tpl<const way_obj_desc_t *>& get_list() { return table; }
};

#endif
