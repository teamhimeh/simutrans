#ifndef schedule_entry_t_h
#define schedule_entry_t_h

#include "koord3d.h"

/**
 * A schedule entry.
 * @author Hj. Malthaner
 */
struct schedule_entry_t
{
public:
	schedule_entry_t() {}

	schedule_entry_t(koord3d const& pos, uint const minimum_loading, sint8 const waiting_time_shift, sint16 spacing_shift, bool wait_for_time) :
		pos(pos),
		minimum_loading(minimum_loading),
		waiting_time_shift(waiting_time_shift),
		spacing_shift(spacing_shift),
		wait_for_time(wait_for_time)
	{}

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
	 * variables for wait_for_time feature brought from simutrans-extended
	 * @brought by THLeaderH
	 */
	 sint16 spacing_shift;
	 // Whether a convoy must wait for a time slot at this entry.
	 bool wait_for_time;
};

inline bool operator ==(const schedule_entry_t &a, const schedule_entry_t &b)
{
	return a.pos == b.pos  &&  a.minimum_loading == b.minimum_loading  &&  a.waiting_time_shift == b.waiting_time_shift  &&  a.spacing_shift == b.spacing_shift  &&  a.wait_for_time == b.wait_for_time;
}


#endif
