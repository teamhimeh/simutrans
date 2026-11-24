//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for pedestrians
//


// Passengers arriving at a stop create pedestrians on the road tiles around the stop.
// The road tile (0, 2) at the map border is a two-way road, but two one-way restrictions in opposite
// directions leave nowhere to go. A pedestrian created there must not look for a tile outside of the map.
function test_pedestrian_arrive_at_dead_road_on_map_border()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_one_way())[0]

	ASSERT_EQUAL(command_x.build_road(pl, coord3d(0, 0, 0), coord3d(0, 10, 0), road_desc, false, {overtaking_mode = twoway_mode}), null)
	ASSERT_EQUAL(command_x.build_road(pl, coord3d(0, 3, 0), coord3d(0, 1, 0), road_desc, false, {overtaking_mode = oneway_mode}), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(0, 2, 0), sign), null)
	ASSERT_EQUAL(tile_x(0, 2, 0).get_way_dirs(wt_road), dir.northsouth)
	ASSERT_EQUAL(tile_x(0, 2, 0).get_way_dirs_masked(wt_road), dir.none)

	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(0, 3, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(0, 7, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(0, 10, 0), building_desc_x("CarDepot")), null)

	local to_halt = halt_x.get_halt(coord3d(0, 3, 0), pl)
	local depot = depot_x(0, 10, 0)
	local bus = vehicle_desc_x("Buessig")

	// the bus brings passengers to the stop next to the dead road tile
	// (it cannot come back through the one-way section)
	depot.append_vehicle(pl, convoy_x(0), bus)
	local cnv = depot.get_convoy_list()[0]
	cnv.change_schedule(pl, schedule_x(wt_road, [ schedule_entry_x(coord3d(0, 7, 0), 0, 0), schedule_entry_x(coord3d(0, 3, 0), 0, 0) ]))
	depot.start_all_convoys(pl)

	sleep()
	sleep()

	// several packets of passengers: each of them creates pedestrians next to the stop
	local num_packets = 4
	local pax_per_packet = bus.get_capacity() / num_packets
	for (local i = 0; i < num_packets; ++i) {
		ASSERT_EQUAL(world.generate_goods(coord(1, 6 + i % 3), coord(1, 2 + i % 3), good_desc_x.passenger, pax_per_packet), 1)
	}
	local num_pax = num_packets * pax_per_packet

	// wait until the passengers are delivered
	local arrived = 0
	for (local i = 0; i < 1000  &&  arrived < num_pax; ++i) {
		sleep()
		arrived = to_halt.arrived[0]
	}
	ASSERT_EQUAL(arrived, num_pax)
	sleep()

	// clean up
	cnv.destroy(pl)
	sleep()
	sleep()
	// remove everything on the tiles (pedestrians, stop, sign, way and depot are removed one after the other)
	for (local y = 10; y >= 0; --y) {
		for (local pass = 0; pass < 20  &&  !tile_x(0, y, 0).is_empty(); ++pass) {
			command_x(tool_remover).work(pl, coord3d(0, y, 0))
		}
		ASSERT_TRUE(tile_x(0, y, 0).is_empty())
	}
	RESET_ALL_PLAYER_FUNDS()
}
