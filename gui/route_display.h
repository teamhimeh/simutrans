/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_ROUTE_DISPLAY_H
#define GUI_ROUTE_DISPLAY_H

#include "../tpl/vector_tpl.h"
#include "../dataobj/koord3d.h"

class schedule_t;
class player_t;

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


/**
 * Overlay that shows the whole route of a schedule (every leg between
 * consecutive stops) on the map and minimap. The route itself is computed
 * by karte_t::step_schedule_route() - which only runs from karte_t::step(),
 * never from sync_step() - so the tiles appear a game step after show().
 *
 * Air schedules are not supported: air_vehicle_t finds its route on its own
 * and calc_route() returns nothing for air_wt, so callers must not offer the
 * button for air_wt schedules.
 *
 * Uses route_display_t so only one route overlay (this one or a "show route"
 * button elsewhere) is visible at a time.
 */
class schedule_route_overlay_t {
public:
	schedule_route_overlay_t() : active_win(0), shown(false), last_seen(0xFFFFFFFFu) {}
	~schedule_route_overlay_t() { hide(); }

	// request the calculation and take over the map overlay
	void show(schedule_t *schedule, player_t *pl, uint16 speed_kmh, bool needs_electrification, void *win, route_display_t::hide_func_t hide_cb);

	// release the overlay and drop the pending/served route
	void hide();

	// pick up the route computed by karte_t::step_schedule_route() and (re)mark
	// the tiles; call once per draw() while the overlay is shown
	void poll();

	bool is_shown() const { return shown; }

private:
	void unmark();
	uint32 owner_id() const { return (uint32)(size_t)this; }

	vector_tpl<koord3d> marked; ///< tiles currently flagged on the map
	void  *active_win;          ///< owner passed to route_display_t::activate()
	bool   shown;
	uint32 last_seen;           ///< tile count of the route we last marked
};

#endif
