/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string.h>
#include "../simdebug.h"
#include "../simtool.h"
#include "simwin.h"
#include "../simworld.h"

#include "../dataobj/translator.h"
#include "../dataobj/settings.h"
#include "../network/network_cmd_ingame.h"

#include "../utils/cbuffer_t.h"
#include "../utils/sha1.h"
#include "../utils/simstring.h"


#include "password_frame.h"
#include "player_frame_t.h"


password_frame_t::password_frame_t( player_t *player ) :
	gui_frame_t( translator::translate("Enter Password"), player )
{
	set_table_layout(2,0);

	this->player = player;

	if(  !player->is_locked()  ||  (welt->get_active_player_nr()==PUBLIC_PLAYER_NR  &&  !welt->get_public_player()->is_locked())   ) {
		// allow to change name name
		tstrncpy( player_name_str, player->get_name(), lengthof(player_name_str) );
		player_name.set_text(player_name_str, lengthof(player_name_str));
		player_name.add_listener(this);
		add_component(&player_name, 2);
	}
	else {
		const_player_name.set_text( player->get_name() );
		add_component(&const_player_name, 2);
	}
	fnlabel.set_text( "Password" ); // so we have a width now
	add_component(&fnlabel);

	// Input box for password
	ibuf[0] = 0;
	password.set_text(ibuf, lengthof(ibuf) );
	password.add_listener(this);
	add_component( &password );
	set_focus( &password );

	// Unlock button: clears the password (unlocks the player slot)
	unlock_button.init( button_t::roundbox | button_t::flexible, "Clear Password" );
	unlock_button.add_listener(this);
	// enabled when: player is locked AND
	//   (active player is this player, OR allow_unlock_by_public && active is public player (unlocked))
	const bool active_is_this   = welt->get_active_player_nr() == player->get_player_nr();
	const bool public_can_unlock = welt->get_settings().get_allow_unlock_by_public()
	                               &&  welt->get_active_player_nr() == PUBLIC_PLAYER_NR
	                               &&  !welt->get_public_player()->is_locked();
	unlock_button.enable(  player->is_password_hash()  &&  ( (!player->is_locked()  &&  active_is_this)  ||  public_can_unlock) );
	add_component( &unlock_button, 2 );

	reset_min_windowsize();
	set_windowsize(get_min_windowsize() );
}




/**
 * This method is called if an action is triggered
 */
bool password_frame_t::action_triggered( gui_action_creator_t *comp, value_t p )
{
	if(comp == &password  &&  (ibuf[0]!=0  ||  p.i == 1)) {
		if (player->is_unlock_pending()) {
			// unlock already pending, do not do everything twice
			return true;
		}
		// Enter-Key pressed
		// test for matching password to unlock
		size_t len = strlen( password.get_text() );

		pwd_hash_t hash;
		// remove hash to re-open slot if password is empty
		if(len>0) {
			SHA1 sha1;
			sha1.Input( password.get_text(), len );
			sha1.Result(hash);
		}

		// store the hash
		welt->store_player_password_hash( player->get_player_nr(), hash );

		const bool public_can_bypass = welt->get_active_player_nr()==PUBLIC_PLAYER_NR
		                               &&  !welt->get_public_player()->is_locked()
		                               &&  welt->get_settings().get_allow_unlock_by_public();

		if(  env_t::networkmode) {
			// block public player bypass (empty hash = no password entered) when allow_unlock_by_public is disabled;
			// a non-empty hash is a genuine password attempt and must reach the server for normal verification
			if(  hash.empty()
			     &&  player->is_locked()
			     &&  welt->get_active_player_nr()==PUBLIC_PLAYER_NR
			     &&  !welt->get_public_player()->is_locked()
			     &&  !welt->get_settings().get_allow_unlock_by_public()  ) {
				return true;
			}
			player->unlock(!player->is_locked(), true);
			// send hash to server: it will unlock player or change password
			nwc_auth_player_t *nwc = new nwc_auth_player_t(player->get_player_nr(), hash);
			network_send_server(nwc);
		}
		else {
			/* if current active player is player 1 and this is unlocked, he may reset passwords
			 * otherwise you need the valid previous password
			 */
			if(  !player->is_locked()  ||  public_can_bypass  ) {
				// set password
				player->access_password_hash() = hash;
				player->unlock(true, false);
			}
			else {
				player->check_unlock(hash);
			}
		}
	}

	if(  comp == &player_name  ) {
		// rename a player
		cbuffer_t buf;
		buf.printf( "p%u,%s", player->get_player_nr(), player_name.get_text() );
		tool_t *tmp_tool = create_tool( TOOL_RENAME | SIMPLE_TOOL );
		tmp_tool->set_default_param( buf );
		welt->set_tool( tmp_tool, player );
		// since init always returns false, it is safe to delete immediately
		delete tmp_tool;
	}

	if(  comp == &unlock_button  ) {
		// clear password hash to unlock the player
		pwd_hash_t empty_hash;
		welt->store_player_password_hash( player->get_player_nr(), empty_hash );
		if(  env_t::networkmode  ) {
			player->unlock(true, true);
			nwc_auth_player_t *nwc = new nwc_auth_player_t(player->get_player_nr(), empty_hash);
			network_send_server(nwc);
		}
		else {
			player->access_password_hash() = empty_hash;
			player->unlock(true, false);
		}
		destroy_win(this);
		return true;
	}

	if(  p.i==1  ) {
		// destroy window after enter is pressed
		destroy_win(this);
	}
	return true;
}
