//
// Regression test: a priority signal placed right in front of a level crossing
// must not brake the convoy.
//
// A priority signal is re-checked on every tile while the convoy is next to it
// (rail_vehicle_t::can_enter_tile). is_priority_signal_clear() used to pull
// next_stop_index back in front of the crossing on every one of those re-checks,
// undoing the advance that can_enter_tile() had just made after a successful
// request_crossing(). The convoy was then permanently inside the brake countdown:
// instead of running at the crossing's own speed limit (100 km/h) it was braking
// towards a stop and got down to 50 km/h while crossing.
//
// Layout (column x=5):
//   y=0   depot
//   y=1   waypoint
//   y=7   priority signal
//   y=8   level crossing (road x=3..7)
//   y=12  simple signal   (needed: without a further signal the whole route is
//                          reserved to the end and the signal is never re-checked)
//   y=15  station
//
// The rail type and the locomotive are pinned by name on purpose: the speed threshold
// at the end only makes sense for a convoy that reaches well over 100 km/h within
// these 7 tiles. Depot and station are irrelevant to it and are looked up generically.
//

function test_priority_signal_before_crossing_no_slowdown()
{
	local pl = player_x(0)
	local rail = way_desc_x("steel_sleeper_track")
	local road = way_desc_x("cobblestone_road")
	local loco = vehicle_desc_x("SD40-2-front")
	local depot_desc = get_depot_by_wt(wt_rail)
	local station_desc = building_desc_x.get_available_stations(
	                         building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
	ASSERT_TRUE(rail != null)
	ASSERT_TRUE(road != null)
	ASSERT_TRUE(loco != null)
	ASSERT_TRUE(depot_desc != null)
	ASSERT_TRUE(station_desc != null)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 0, 0), coord3d(5, 15, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 8, 0), coord3d(7, 8, 0), road, true), null)
	// building a road across the track must have created a crossing
	ASSERT_TRUE(tile_x(5, 8, 0).find_object(mo_crossing) != null)

	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(5, 0, 0), depot_desc), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(5, 15, 0), station_desc), null)

	local pri_desc = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, s) s.is_priority_signal())[0]
	local sig_desc = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, s) s.is_signal())[0]
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(5, 7, 0), pri_desc), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(5, 12, 0), sig_desc), null)

	local depot = depot_x(5, 0, 0)
	depot.append_vehicle(pl, convoy_x(0), loco)
	local cnv = depot.get_convoy_list()[0]
	cnv.change_schedule(pl, schedule_x(wt_rail, [
		schedule_entry_x(coord3d(5, 15, 0), 0, 0),
		schedule_entry_x(coord3d(5, 1, 0), 0, 0)
	]))
	debug.set_game_speed(8)
	depot.start_all_convoys(pl)

	// slowest speed seen while running over the crossing and the tiles right after it
	local min_speed = 32767
	local samples = 0
	local reached = false
	for (local i = 0; i < 1500; i++) {
		if (!cnv.is_valid()) break
		local p = cnv.get_pos()
		if (p.x == 5 && p.y >= 8 && p.y <= 11) {
			local s = cnv.get_speed()
			if (s < min_speed) { min_speed = s }
			samples++
		}
		if (p.y >= 12) { reached = true; break }
		sleep()
	}
	debug.set_game_speed(1)

	// clean up before asserting, so that following tests find the tiles free even on failure.
	// test_otrp_signal_options builds its own track and platforms on x=5, y=8..11.
	if (cnv.is_valid()) {
		cnv.destroy(pl)
		for (local i = 0; i < 200 && cnv.is_valid(); i++) sleep()
	}
	foreach (y in [7, 12]) {
		if (tile_x(5, y, 0).find_object(mo_signal) != null) {
			command_x(tool_remover).work(pl, coord3d(5, y, 0))
		}
	}
	command_x(tool_remover).work(pl, coord3d(5, 15, 0))  // station
	command_x(tool_remover).work(pl, coord3d(5, 0, 0))   // depot
	// the road takes the crossing with it
	command_x(tool_remove_way).work(pl, coord3d(3, 8, 0), coord3d(7, 8, 0), "" + wt_road)
	command_x(tool_remove_way).work(pl, coord3d(5, 0, 0), coord3d(5, 15, 0), "" + wt_rail)

	ASSERT_TRUE(reached)
	// without a single sample the speed check below would pass vacuously
	ASSERT_GREATER(samples, 0)
	// unimpeded the convoy runs at >110 km/h here; with the bug it was capped at 50 km/h
	ASSERT_GREATER(min_speed, 80)
}
