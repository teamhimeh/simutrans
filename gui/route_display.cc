/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "route_display.h"
#include <cstddef>

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
