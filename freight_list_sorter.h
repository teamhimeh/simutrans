/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef FREIGHT_LIST_SORTER_H
#define FREIGHT_LIST_SORTER_H


// same sorting for stations and vehicle/convoi freight ...

#include "simtypes.h"

template<class T> class slist_tpl;
template<class T> class vector_tpl;
class ware_t;
class goods_desc_t;
class cbuffer_t;
class karte_ptr_t;


class freight_list_sorter_t
{
public:
	enum sort_mode_t {
		by_name    = 0,
		by_via     = 1,
		by_via_sum = 2,
		by_amount  = 3
	};

	/**
	 * One entry of the capacity list: a good and the total capacity for it.
	 * The total is kept apart from ware_t, since ware_t::menge is too narrow
	 * for the sum over a whole convoy.
	 */
	struct capacity_t {
		const goods_desc_t *desc;
		sint32 amount;
	};

	static void sort_freight(vector_tpl<ware_t> const& warray, cbuffer_t& buf, sort_mode_t sort_mode, const slist_tpl<capacity_t>* full_list, const char* what_doing);

private:
	static karte_ptr_t welt;

	static sort_mode_t sortby;

	static bool compare_ware(ware_t const& w1, ware_t const& w2);

	static void add_ware_heading( cbuffer_t &buf, uint64 sum, sint32 max, const goods_desc_t *desc, const char *what_doing );
};


#endif
