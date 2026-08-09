/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "privatesign_info.h"
#include "components/gui_label.h"
#include "../obj/roadsign.h"
#include "../player/simplay.h"


#include "../simmenu.h"
#include "../simworld.h"

privatesign_info_t::privatesign_info_t(roadsign_t* s) :
	obj_infowin_t(s),
	sign(s)
{
	for(  int i=0;  i<PLAYER_UNOWNED;  i++  ) {
		if(  welt->get_player(i)  ) {
			players[i].init( button_t::square_state, welt->get_player(i)->get_name());
			players[i].add_listener( this );
		}
		else {
			players[i].init( button_t::square_state, "");
			players[i].disable();
		}
		players[i].pressed = (sign->get_player_mask() & ((uint64)1 << i)) != 0;
		add_component( &players[i] );
	}

	// show author below the settings
	if (char const* const maker = sign->get_desc()->get_copyright()) {
		gui_label_buf_t* lb = new_component<gui_label_buf_t>();
		lb->buf().printf(translator::translate("Constructed by %s"), maker);
		lb->update();
	}

	recalc_size();
}


/**
 * This method is called if an action is triggered
 *
 * Returns true, if action is done and no more
 * components should be triggered.
 */
bool privatesign_info_t::action_triggered( gui_action_creator_t *comp, value_t /* */)
{
	if(  welt->get_active_player() ==  sign->get_owner()  ) {
		char param[256];
		for(  int i=0;  i<PLAYER_UNOWNED;  i++  ) {
			if(comp == &players[i]) {
				uint64 new_mask = sign->get_player_mask() ^ ((uint64)1 << i);
				sint32 mask_lo = (sint32)(new_mask & 0xFFFFFFFFULL);
				sint32 mask_hi = (sint32)(new_mask >> 32);
				sprintf( param, "%s,5,0,%d,%d", sign->get_pos().get_str(), mask_lo, mask_hi );
				tool_t::simple_tool[TOOL_CHANGE_TRAFFIC_LIGHT]->set_default_param( param );
				welt->set_tool( tool_t::simple_tool[TOOL_CHANGE_TRAFFIC_LIGHT], welt->get_active_player() );
				players[i].pressed = (new_mask >> i) & 1;
			}
		}
	}
	return true;
}


// notify for an external update
void privatesign_info_t::update_data()
{
	for(  int i=0;  i<PLAYER_UNOWNED;  i++  ) {
		players[i].pressed = (sign->get_player_mask() & ((uint64)1 << i)) != 0;
	}
}
