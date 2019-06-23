#ifndef schedule_entry_t_h
#define schedule_entry_t_h

#include "koord3d.h"
#include "../linehandle_t.h"

/**
 * A schedule entry.
 * @author Hj. Malthaner
 */
struct schedule_entry_t
{
public:
	schedule_entry_t() {}

	schedule_entry_t(sint16 id, koord3d const& pos, uint const minimum_loading, sint8 const waiting_time_shift) :
		id(id),
		pos(pos),
		minimum_loading(minimum_loading),
		waiting_time_shift(waiting_time_shift),
		child_line(linehandle_t()),
		child_entry_id(-1),
		parent_line(linehandle_t()),
		parent_entry_id(-1)
	{}
		
	/**
	 * entry id in the schedule
	 * @author THLeaderH
	 */
	sint16 id;

	/**
	 * target position
	 * @author Hj. Malthaner
	 */
	koord3d pos;

	/**
	 * Wait for % load at this stops
	 * (ignored on waypoints)
	 * @author Hj. Malthaner
	 */
	uint8 minimum_loading;

	/**
	 * maximum waiting time in 1/2^(16-n) parts of a month
	 * (only active if minimum_loading!=0)
	 * @author prissi
	 */
	sint8 waiting_time_shift;
	
	/**
	 * entries related to coupling and decoupling
	 * child_line: A convoy of child_line follows this convoy when coupling.
	 * parent_line: This convoy follows the convoy of parent_line when coupling.
	 * @author THLeaderH
	 */
	linehandle_t child_line;
	sint16 child_entry_id;
	linehandle_t parent_line;
	sint16 parent_entry_id;
};

inline bool operator ==(const schedule_entry_t &a, const schedule_entry_t &b)
{
	return a.id == b.id  &&  a.pos == b.pos  &&  a.minimum_loading == b.minimum_loading  &&  a.waiting_time_shift == b.waiting_time_shift  &&  a.child_line == b.child_line  &&  a.parent_line == b.parent_line;
}


#endif
