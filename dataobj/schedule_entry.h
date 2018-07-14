#ifndef schedule_entry_h
#define schedule_entry_h

#include "koord3d.h"
#include "../linehandle_t.h"

/**
 * A schedule entry.
 * @author Hj. Malthaner
 */
struct schedule_entry_t
{
	/**
	* Operation types
	* normal: normal stop on a halt or passing a waypoint.
	* merge: a slave convoy merges to the master convoy.
	* couple: a master convoy waits for the slave convoy.
	* release: a coupled convoy releases the slave convoy.
	* @author THLeaderH
	*/
	enum operation_type{ normal, merge, couple, release };

public:
	schedule_entry_t() {}

	schedule_entry_t(koord3d const& pos, uint16 const minimum_loading, sint8 const waiting_time_shift, sint16 spacing_shift, sint8 reverse, bool wait_for_time, operation_type operation = normal) :
		pos(pos),
		minimum_loading(minimum_loading),
		spacing_shift(spacing_shift),
		waiting_time_shift(waiting_time_shift),
		reverse(reverse),
		wait_for_time(wait_for_time),
		operation(operation)
	{
		destination_line = linehandle_t(NULL);
		destination_index = -1;
	}

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
	uint16 minimum_loading;

	/**
	 * spacing shift
	 * @author Inkelyad
	 */
	sint16 spacing_shift;

	/**
	* maximum waiting time in 1/2^(16-n) parts of a month
	* (only active if minimum_loading!=0)
	* @author prissi
	*/
	sint8 waiting_time_shift;

	/**
	 * Whether a convoy needs to reverse after this entry.
	 * 0 = no; 1 = yes; -1 = undefined
	 * @author: jamespetts
	 */
	sint8 reverse;

	/**
	 * Whether a convoy must wait for a
	 * time slot at this entry.
	 * @author: jamespetts
	 */
	bool wait_for_time;

	/*
	 * Following parameters are used for coupling and release of convoy.
	 * @author THLeaderH
	 */
	operation_type operation;
	linehandle_t destination_line;
	sint16 destination_index;

};

#endif
