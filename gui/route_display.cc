/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "route_display.h"
#include <cstddef>

#include "minimap.h"
#include "../simworld.h"
#include "../obj/simobj.h"
#include "../boden/grund.h"
#include "../dataobj/schedule.h"

static karte_ptr_t rd_welt;


const char *format_route_time_hours( uint32 ticks )
{
	static char buf[64];
	uint64 const ticks_per_month = (uint64)rd_welt->ticks_per_world_month;
	uint64 const divisor = (uint64)world()->get_settings().get_spacing_shift_divisor();
	uint64 ticks_write = (uint64)ticks*divisor/ticks_per_month;
	if(  ticks_write >= divisor  ) {
		sprintf( buf, "%llu (%i d)", ticks_write, ticks/divisor );
	}
	else {
		sprintf( buf, "%llu ", ticks_write );
	}
	return buf;
}

void *route_display_t::active_owner = NULL;
route_display_t::hide_func_t route_display_t::active_hide = NULL;

void route_display_t::activate(void *owner, hide_func_t hide)
{
	if (active_owner && active_owner != owner && active_hide) {
		active_hide(active_owner);
	}
	active_owner = owner;
	active_hide = hide;
}

void route_display_t::deactivate(void *owner)
{
	if (active_owner == owner) {
		active_owner = NULL;
		active_hide = NULL;
	}
}


bool schedule_route_overlay_t::route_ready() const
{
	// this overlay owns the shared route and step() has already calculated it
	return shown
		&&  rd_welt->get_schedule_route_owner() == owner_id()
		&&  !rd_welt->is_schedule_route_pending();
}


void schedule_route_overlay_t::unmark()
{
	for(  uint32 i = 0;  i < marked.get_count();  i++  ) {
		if(  grund_t* const gr = rd_welt->lookup( marked[i] )  ) {
			for(  uint idx = 0;  idx < gr->get_top();  idx++  ) {
				gr->obj_bei( idx )->clear_flag( obj_t::convoy_way );
			}
			gr->set_flag( grund_t::dirty );
		}
	}
	marked.clear();
}


void schedule_route_overlay_t::show(schedule_t *schedule, player_t *pl, uint16 speed_kmh, bool needs_electrification, void *win, route_display_t::hide_func_t hide_cb)
{
	if(  schedule == NULL  ||  pl == NULL  ) {
		return;
	}
	// order matters: claim ownership of the shared route before route_display_t
	// tells the previous overlay to hide (its clear_schedule_route() then sees a
	// newer owner and leaves our request alone)
	rd_welt->request_schedule_route( schedule, pl, owner_id(), speed_kmh, needs_electrification );
	route_display_t::activate( win, hide_cb );
	active_win = win;
	shown = true;
	last_seen = 0xFFFFFFFFu; // force poll() to (re)mark once the route is ready
	unmark();
	minimap_t::get_instance()->clear_highlighted_route();
}


void schedule_route_overlay_t::hide()
{
	unmark();
	minimap_t::get_instance()->clear_highlighted_route();
	rd_welt->clear_schedule_route( owner_id() );
	if(  active_win  ) {
		route_display_t::deactivate( active_win );
		active_win = NULL;
	}
	shown = false;
	last_seen = 0xFFFFFFFFu;
}


void schedule_route_overlay_t::poll()
{
	if(  !shown  ) {
		return;
	}
	// another overlay took over the shared route: route_display_t will have
	// asked us to hide already, so just do not touch its tiles
	if(  rd_welt->get_schedule_route_owner() != owner_id()  ) {
		return;
	}
	const vector_tpl<koord3d> &route = rd_welt->get_schedule_route();

	// re-apply every frame, exactly like convoi_info_t::show_route(): vehicles
	// passing over a tile clear the convoy_way flag again, so the line route is
	// drawn the same way a convoy route is
	last_seen = route.get_count();
	unmark();
	for(  uint32 i = 0;  i < route.get_count();  i++  ) {
		const koord3d pos = route[i];
		if(  pos == koord3d::invalid  ) {
			continue; // leg separator
		}
		if(  grund_t* const gr = rd_welt->lookup( pos )  ) {
			for(  uint idx = 0;  idx < gr->get_top();  idx++  ) {
				obj_t *obj = gr->obj_bei( idx );
				if(  !obj->is_moving()  ) {
					obj->set_flag( obj_t::convoy_way );
				}
			}
			gr->set_flag( grund_t::dirty );
			marked.append( pos );
		}
	}
	minimap_t::get_instance()->set_highlighted_route( marked );
}
