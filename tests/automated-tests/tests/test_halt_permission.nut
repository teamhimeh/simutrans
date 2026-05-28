//
// Tests for halt stop-permission system (OTRP per-player permissions).
//
// NOTE: halt.toggle_allow_other_player() and halt.set_halt_permissions() use
// call_tool_init which returns true (not null) on success in non-network mode.


function test_halt_permission_default_owner_only()
{
	local pl      = player_x(0)
	local pub     = player_x(1)
	local road    = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), station), null)

	local halt = halt_x.get_halt(coord3d(4, 3, 0), pl)
	ASSERT_TRUE(halt != null)

	// Default: only owner bit should be set; allow_all flag should be off
	local owner_bit = 1 << pl.get_player_nr()
	ASSERT_EQUAL(halt.get_permissions(), owner_bit)
	ASSERT_EQUAL(halt.is_allow_other_player_connection(), false)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_permission_toggle_allow_all_on()
{
	local pl      = player_x(0)
	local road    = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), station), null)

	local halt = halt_x.get_halt(coord3d(4, 3, 0), pl)
	ASSERT_TRUE(halt != null)

	// Toggle allow-all ON
	ASSERT_TRUE(halt.toggle_allow_other_player(pl))

	ASSERT_EQUAL(halt.get_permissions(), 0xFFFF)
	ASSERT_EQUAL(halt.is_allow_other_player_connection(), true)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_permission_toggle_allow_all_off_keeps_all_permitted()
{
	local pl      = player_x(0)
	local road    = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), station), null)

	local halt = halt_x.get_halt(coord3d(4, 3, 0), pl)
	ASSERT_TRUE(halt != null)

	// Toggle allow-all ON then OFF
	ASSERT_TRUE(halt.toggle_allow_other_player(pl))
	ASSERT_TRUE(halt.toggle_allow_other_player(pl))

	// Flag is off but permissions remain 0xFFFF (all permitted, per-player edit mode)
	ASSERT_EQUAL(halt.get_permissions(), 0xFFFF)
	ASSERT_EQUAL(halt.is_allow_other_player_connection(), false)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_permission_set_per_player()
{
	local pl      = player_x(0)
	local road    = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), station), null)

	local halt = halt_x.get_halt(coord3d(4, 3, 0), pl)
	ASSERT_TRUE(halt != null)

	local owner_bit = 1 << pl.get_player_nr()
	// Allow player 2 in addition to owner
	local new_perms = owner_bit | (1 << 2)
	ASSERT_TRUE(halt.set_halt_permissions(pl, new_perms))

	// Owner bit is always forced on; player 2 bit should be set
	ASSERT_EQUAL(halt.get_permissions(), new_perms)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_permission_non_owner_cannot_change()
{
	// Public player builds halt; player 0 (non-owner) tries to change — should fail silently.
	// check_owner(pub, pl0) == false since pl0 is not public player.
	local pl      = player_x(0)
	local pub     = player_x(1)
	local road    = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pub, coord3d(4, 2, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_station(pub, coord3d(4, 3, 0), station), null)

	local halt = halt_x.get_halt(coord3d(4, 3, 0), pub)
	ASSERT_TRUE(halt != null)
	local perms = halt.get_permissions()

	// Player 0 attempts to change permissions of public player's halt — should have no effect
	local new_perms = 0x0001
	ASSERT_TRUE(halt.set_halt_permissions(pl, new_perms))
	ASSERT_EQUAL(halt.get_permissions(), perms)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pub, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_permission_public_halt_always_all()
{
	local pl      = player_x(0)
	local pub     = player_x(1)
	local road    = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), station), null)
	ASSERT_EQUAL(command_x(tool_make_stop_public).work(pl, coord3d(4, 3, 0)), null)

	local halt = halt_x.get_halt(coord3d(4, 3, 0), pub)
	ASSERT_TRUE(halt != null)

	// Public halts always have 0xFFFF permissions
	ASSERT_EQUAL(halt.get_permissions(), 0xFFFF)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pub, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}
