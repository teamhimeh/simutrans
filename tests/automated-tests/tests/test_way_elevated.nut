//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for elevated way building / removal
//


function prepare_other_player()
{
	if (!player_x(2).is_valid()) {
		ASSERT_TRUE(world.create_player(2, 1))
	}
	return player_x(2)
}


function test_way_elevated_build_over_other_player_halt_setting_off()
{
	local pl = player_x(0)
	local other_pl = prepare_other_player()
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local elevated_desc = way_desc_x.get_available_ways(wt_monorail, st_elevated)[0]
	local station_desc = building_desc_x("BusStop")

	ASSERT_TRUE(road_desc != null)
	ASSERT_TRUE(elevated_desc != null)
	ASSERT_TRUE(station_desc != null)

	settings.set_allow_elevated_way_over_others_halt(false)
	ASSERT_FALSE(settings.get_allow_elevated_way_over_others_halt())

	ASSERT_EQUAL(command_x.build_way(other_pl, coord3d(3, 2, 0), coord3d(3, 4, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_station(other_pl, coord3d(3, 3, 0), station_desc), null)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 2, 0), coord3d(3, 4, 0), elevated_desc, false), null)

	local elevated_tile = square_x(3, 3).get_tile_at_height(1)
	ASSERT_TRUE(elevated_tile == null  ||  elevated_tile.find_object(mo_way) == null)
}


function test_way_elevated_build_over_other_player_halt_setting_on()
{
	local pl = player_x(0)
	local other_pl = prepare_other_player()
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local elevated_desc = way_desc_x.get_available_ways(wt_monorail, st_elevated)[0]
	local station_desc = building_desc_x("BusStop")

	ASSERT_TRUE(road_desc != null)
	ASSERT_TRUE(elevated_desc != null)
	ASSERT_TRUE(station_desc != null)

	ASSERT_EQUAL(command_x.build_way(other_pl, coord3d(3, 2, 0), coord3d(3, 4, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_station(other_pl, coord3d(3, 3, 0), station_desc), null)

	settings.set_allow_elevated_way_over_others_halt(true)
	ASSERT_TRUE(settings.get_allow_elevated_way_over_others_halt())

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 2, 0), coord3d(3, 4, 0), elevated_desc, true), null)
	ASSERT_TRUE(square_x(3, 3).get_tile_at_height(1).find_object(mo_way) != null)

}


function test_way_elevated_build_over_other_player_non_halt_forbidden()
{
	local pl = player_x(0)
	local other_pl = prepare_other_player()
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local elevated_desc = way_desc_x.get_available_ways(wt_monorail, st_elevated)[0]
	local depot_desc = building_desc_x("CarDepot")

	ASSERT_TRUE(road_desc != null)
	ASSERT_TRUE(elevated_desc != null)
	ASSERT_TRUE(depot_desc != null)

	ASSERT_EQUAL(command_x.build_way(other_pl, coord3d(3, 3, 0), coord3d(3, 4, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_depot(other_pl, coord3d(3, 3, 0), depot_desc), null)

	settings.set_allow_elevated_way_over_others_halt(true)
	ASSERT_TRUE(settings.get_allow_elevated_way_over_others_halt())

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 2, 0), coord3d(3, 4, 0), elevated_desc, true), "")

}
