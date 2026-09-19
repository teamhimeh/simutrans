/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "way_desc.h"
#include "way_obj_desc.h"
#include "../network/checksum.h"
#include "../simworld.h"
#include "../dataobj/settings.h"


waytype_t way_desc_t::get_finance_waytype() const
{
	return get_styp() == type_tram ? tram_wt : get_wtyp();
}


sint64 way_desc_t::get_maintenance() const
{
	const sint64 base = obj_desc_transport_related_t::get_maintenance();
	if(  world() == NULL  ) {
		// no world yet (e.g. while loading the pakset): no scaling possible
		return base;
	}
	return (base * (sint64)world()->get_settings().get_maintenance_cost_multiplier_way()) / 100ll;
}


sint64 way_obj_desc_t::get_maintenance() const
{
	const sint64 base = obj_desc_transport_related_t::get_maintenance();
	if(  world() == NULL  ) {
		// no world yet (e.g. while loading the pakset): no scaling possible
		return base;
	}
	return (base * (sint64)world()->get_settings().get_maintenance_cost_multiplier_overhead()) / 100ll;
}


void way_desc_t::calc_checksum(checksum_t *chk) const
{
	obj_desc_transport_infrastructure_t::calc_checksum(chk);
	chk->input(max_weight);
	chk->input(styp);
	chk->input(has_double_slopes());
}
