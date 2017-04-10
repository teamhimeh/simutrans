/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include "onewaysign_info.h"
#include "../obj/roadsign.h"
#include "../player/simplay.h"


#include "../simmenu.h"
#include "../simworld.h"

onewaysign_info_t::onewaysign_info_t(roadsign_t* s, koord3d first_intersection) :
	obj_infowin_t(s),
	sign(s)
{
	direction[0].init( button_t::square_state, translator::translate("Left"), scr_coord(10,get_windowsize().h - 40), scr_size(40,D_BUTTON_HEIGHT) );
	direction[1].init( button_t::square_state, translator::translate("Right"), scr_coord(60,get_windowsize().h - 40), scr_size(40,D_BUTTON_HEIGHT) );
	direction[0].add_listener( this );
	direction[1].add_listener( this );
	direction[0].pressed = (sign->get_lane_fix() & 1) != 0;
	direction[1].pressed = (sign->get_lane_fix() & 2) != 0;
	add_component( &direction[0] );
	add_component( &direction[1] );
}


/**
 * This method is called if an action is triggered
 * @author Hj. Malthaner
 *
 * Returns true, if action is done and no more
 * components should be triggered.
 * V.Meyer
 */
bool onewaysign_info_t::action_triggered( gui_action_creator_t *komp, value_t /* */)
{
	uint8 fix = sign->get_lane_fix();
	for(  int i=0;  i<2;  i++  ) {
		if(komp == &direction[i]) {
			fix ^= (i+1);
		}
	}
	sign->set_lane_fix(fix);
	for(  int i=0;  i<2;  i++  ) {
		direction[i].pressed = (sign->get_lane_fix() & (i+1)) != 0;
	}
	return true;
}


// notify for an external update
void onewaysign_info_t::update_data()
{
	for(  int i=0;  i<2;  i++  ) {
		direction[i].pressed = (sign->get_lane_fix() & (i+1)) != 0;
	}
}
