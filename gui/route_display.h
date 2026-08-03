/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_ROUTE_DISPLAY_H
#define GUI_ROUTE_DISPLAY_H

/**
 * Ensures that at most one window's route (or route cache) is highlighted
 * on the map at a time. When a window activates its own display, whichever
 * other window was previously showing one gets told to hide it first.
 */
class route_display_t {
public:
	typedef void (*hide_func_t)(void *owner);

	// call when a window turns its route display on
	static void activate(void *owner, hide_func_t hide);

	// call when a window turns its route display off (or is destroyed)
	static void deactivate(void *owner);

private:
	static void *active_owner;
	static hide_func_t active_hide;
};

#endif
