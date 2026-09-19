//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


function foot_test_schedule(stop_positions)
{
	return schedule_x(wt_road, stop_positions.map(@(pos) schedule_entry_x(pos, 0, 0)))
}


function foot_test_find_connection(from_halt, to_halt, freight)
{
	foreach (connection in from_halt.get_connections(freight)) {
		// halt_x wrappers created for the connection table are not object-identical
		// to wrappers returned by halt_x.get_halt(), so compare their stable names.
		if (connection.halt.get_name() == to_halt.get_name()) {
			return connection
		}
	}
	return null
}


function foot_test_enable(route_weight = 100, time_ticks = 1000, time_based = false)
{
	local pax = good_desc_x.passenger
	ASSERT_TRUE(settings.set_transit_by_foot(true))
	ASSERT_TRUE(settings.set_foot_path_weight(route_weight))
	ASSERT_TRUE(settings.set_foot_path_time_ticks(time_ticks))
	ASSERT_TRUE(settings.set_time_based_routing_enabled(pax, time_based))
	ASSERT_TRUE(settings.get_transit_by_foot())
	ASSERT_EQUAL(settings.get_foot_path_weight(), route_weight)
	ASSERT_EQUAL(settings.get_foot_path_time_ticks(), time_ticks)
	ASSERT_EQUAL(settings.get_time_based_routing_enabled(pax), time_based)
}


function foot_test_build_road(pl, from, to)
{
	ASSERT_EQUAL(command_x(tool_build_way).work(pl, from, to, "cobblestone_road"), null)
}


function foot_test_build_stop(pl, pos, name = "BusStop")
{
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, pos, name), null)
}


function foot_test_build_depot(pl, pos)
{
	ASSERT_EQUAL(command_x.build_depot(pl, pos, building_desc_x("CarDepot")), null)
}


function foot_test_start_bus(pl, depot_pos, stop_positions)
{
	local depot = depot_x(depot_pos.x, depot_pos.y, depot_pos.z)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	local convoys = depot.get_convoy_list()
	local cnv = convoys[convoys.len() - 1]
	cnv.change_schedule(pl, foot_test_schedule(stop_positions))
	depot.start_all_convoys(pl)
	return cnv
}


function test_transit_by_foot_connection_range_weight_and_goods()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	local mail = good_desc_x.mail
	foot_test_enable(100, 1000, false)

	// Each of the first two halts accepts both passengers and mail. Their nearest
	// tiles are two columns apart, exactly at the coverage boundary of this map.
	foot_test_build_road(pl, coord3d(2, 1, 0), coord3d(2, 3, 0))
	foot_test_build_stop(pl, coord3d(2, 2, 0), "BusStop")
	foot_test_build_stop(pl, coord3d(2, 3, 0), "PostStop")

	foot_test_build_road(pl, coord3d(4, 1, 0), coord3d(4, 3, 0))
	foot_test_build_stop(pl, coord3d(4, 2, 0), "BusStop")
	foot_test_build_stop(pl, coord3d(4, 3, 0), "PostStop")

	// This halt is outside the coverage square of the halt at x=2.
	foot_test_build_road(pl, coord3d(7, 1, 0), coord3d(7, 3, 0))
	foot_test_build_stop(pl, coord3d(7, 2, 0), "BusStop")

	sleep()
	sleep()

	local halt_a = halt_x.get_halt(coord3d(2, 2, 0), pl)
	local halt_b = halt_x.get_halt(coord3d(4, 2, 0), pl)
	local halt_outside = halt_x.get_halt(coord3d(7, 2, 0), pl)
	ASSERT_TRUE(halt_a.accepts_good(pax))
	ASSERT_TRUE(halt_a.accepts_good(mail))
	ASSERT_TRUE(halt_b.accepts_good(pax))
	ASSERT_TRUE(halt_b.accepts_good(mail))

	local pax_connection = foot_test_find_connection(halt_a, halt_b, pax)
	ASSERT_TRUE(pax_connection != null)
	ASSERT_TRUE(pax_connection.is_foot_path)
	ASSERT_EQUAL(pax_connection.raw_weight, 100 * 2)
	ASSERT_EQUAL(pax_connection.line, null)
	ASSERT_EQUAL(foot_test_find_connection(halt_a, halt_outside, pax), null)
	ASSERT_EQUAL(foot_test_find_connection(halt_a, halt_b, mail), null)

	// Disabling the feature must invalidate and remove the precomputed links.
	ASSERT_TRUE(settings.set_transit_by_foot(false))
	sleep()
	sleep()
	ASSERT_EQUAL(foot_test_find_connection(halt_a, halt_b, pax), null)
}


function test_transit_by_foot_time_based_weight()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable(100, 1000, true)

	foot_test_build_road(pl, coord3d(2, 1, 0), coord3d(2, 3, 0))
	foot_test_build_stop(pl, coord3d(2, 2, 0))
	foot_test_build_road(pl, coord3d(4, 1, 0), coord3d(4, 3, 0))
	foot_test_build_stop(pl, coord3d(4, 2, 0))
	sleep()
	sleep()

	local halt_a = halt_x.get_halt(coord3d(2, 2, 0), pl)
	local halt_b = halt_x.get_halt(coord3d(4, 2, 0), pl)
	local connection = foot_test_find_connection(halt_a, halt_b, pax)
	ASSERT_TRUE(connection != null)
	ASSERT_TRUE(connection.is_foot_path)
	ASSERT_EQUAL(connection.raw_weight, 1000 * 2)
}


function test_transit_by_foot_height_difference()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local pax = good_desc_x.passenger
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local bridge = bridge_desc_x.get_available_bridges(wt_road)[0]
	foot_test_enable(100, 1000, false)

	// Different owners keep the vertically stacked station tiles as separate halts.
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x(tool_build_bridge).work(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), bridge.get_name()), null)
	foot_test_build_stop(pl, coord3d(3, 4, 0))
	foot_test_build_stop(public_pl, coord3d(3, 4, 1))
	sleep()
	sleep()

	local lower_halt = halt_x.get_halt(coord3d(3, 4, 0), pl)
	local upper_halt = halt_x.get_halt(coord3d(3, 4, 1), public_pl)
	ASSERT_TRUE(lower_halt.get_name() != upper_halt.get_name())
	local connection = foot_test_find_connection(lower_halt, upper_halt, pax)
	ASSERT_TRUE(connection != null)
	ASSERT_TRUE(connection.is_foot_path)
	// max(1, abs(dx) + abs(dy)) + abs(dz) = 1 + 1
	ASSERT_EQUAL(connection.raw_weight, 100 * 2)
}


function test_transit_by_foot_vehicle_connection_precedes_walking()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable()

	foot_test_build_road(pl, coord3d(4, 1, 0), coord3d(4, 5, 0))
	foot_test_build_stop(pl, coord3d(4, 2, 0))
	foot_test_build_stop(pl, coord3d(4, 4, 0))
	foot_test_build_depot(pl, coord3d(4, 5, 0))

	ASSERT_TRUE(pl.create_line(wt_road))
	local lines = pl.get_line_list()
	local line = lines[lines.get_count() - 1]
	line.change_schedule(pl, foot_test_schedule([coord3d(4, 2, 0), coord3d(4, 4, 0)]))

	local depot = depot_x(4, 5, 0)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	local cnv = depot.get_convoy_list()[0]
	cnv.set_line(pl, line)
	depot.start_all_convoys(pl)
	sleep()
	sleep()

	local halt_a = halt_x.get_halt(coord3d(4, 2, 0), pl)
	local halt_b = halt_x.get_halt(coord3d(4, 4, 0), pl)
	local active_connection = foot_test_find_connection(halt_a, halt_b, pax)
	ASSERT_TRUE(active_connection != null)
	ASSERT_FALSE(active_connection.is_foot_path)
	ASSERT_TRUE(active_connection.line != null)

	// Keep the line but remove its last convoy. Walking must become the fallback.
	cnv.destroy(pl)
	sleep()
	sleep()
	local fallback_connection = foot_test_find_connection(halt_a, halt_b, pax)
	ASSERT_TRUE(fallback_connection != null)
	ASSERT_TRUE(fallback_connection.is_foot_path)
	ASSERT_EQUAL(fallback_connection.line, null)
}


function test_transit_by_foot_between_two_vehicle_legs()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable()
	debug.set_game_speed(5)

	// A --vehicle--> B, B --foot--> C, C --vehicle--> D
	foot_test_build_road(pl, coord3d(3, 1, 0), coord3d(3, 7, 0))
	foot_test_build_stop(pl, coord3d(3, 2, 0))
	foot_test_build_stop(pl, coord3d(3, 6, 0))
	foot_test_build_depot(pl, coord3d(3, 7, 0))

	foot_test_build_road(pl, coord3d(5, 5, 0), coord3d(5, 14, 0))
	foot_test_build_stop(pl, coord3d(5, 6, 0))
	foot_test_build_stop(pl, coord3d(5, 12, 0))
	foot_test_build_depot(pl, coord3d(5, 14, 0))

	foot_test_start_bus(pl, coord3d(3, 7, 0), [coord3d(3, 2, 0), coord3d(3, 6, 0)])
	foot_test_start_bus(pl, coord3d(5, 14, 0), [coord3d(5, 6, 0), coord3d(5, 12, 0)])
	sleep()
	sleep()

	local halt_a = halt_x.get_halt(coord3d(3, 2, 0), pl)
	local halt_b = halt_x.get_halt(coord3d(3, 6, 0), pl)
	local halt_c = halt_x.get_halt(coord3d(5, 6, 0), pl)
	local halt_d = halt_x.get_halt(coord3d(5, 12, 0), pl)
	local foot_connection = foot_test_find_connection(halt_b, halt_c, pax)
	ASSERT_TRUE(foot_connection != null)
	ASSERT_TRUE(foot_connection.is_foot_path)

	ASSERT_EQUAL(world.generate_goods(coord(1, 2), coord(7, 12), pax, 10), 1)
	ASSERT_EQUAL(halt_a.get_freight_to_halt(pax, halt_b), 10)

	while (halt_d.arrived[0] < 10) {
		sleep()
	}
	ASSERT_EQUAL(halt_a.departed[0], 10)
	ASSERT_EQUAL(halt_b.arrived[0], 10)
	ASSERT_EQUAL(halt_c.departed[0], 10)
	ASSERT_EQUAL(halt_d.arrived[0], 10)
	ASSERT_EQUAL(halt_b.waiting[0], 0)
	debug.set_game_speed(1)
}


function test_transit_by_foot_not_used_as_first_leg()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable()

	// A --foot--> B --vehicle--> C. The origin is covered only by A.
	foot_test_build_road(pl, coord3d(3, 3, 0), coord3d(3, 5, 0))
	foot_test_build_stop(pl, coord3d(3, 4, 0))
	foot_test_build_road(pl, coord3d(5, 3, 0), coord3d(5, 11, 0))
	foot_test_build_stop(pl, coord3d(5, 4, 0))
	foot_test_build_stop(pl, coord3d(5, 9, 0))
	foot_test_build_depot(pl, coord3d(5, 11, 0))
	foot_test_start_bus(pl, coord3d(5, 11, 0), [coord3d(5, 4, 0), coord3d(5, 9, 0)])
	sleep()
	sleep()

	ASSERT_EQUAL(world.generate_goods(coord(1, 4), coord(7, 9), pax, 10), 0)
}


function test_transit_by_foot_not_used_as_last_leg()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable()

	// A --vehicle--> B --foot--> C. The destination is covered only by C.
	foot_test_build_road(pl, coord3d(3, 1, 0), coord3d(3, 8, 0))
	foot_test_build_stop(pl, coord3d(3, 2, 0))
	foot_test_build_stop(pl, coord3d(3, 7, 0))
	foot_test_build_depot(pl, coord3d(3, 8, 0))
	foot_test_build_road(pl, coord3d(5, 6, 0), coord3d(5, 8, 0))
	foot_test_build_stop(pl, coord3d(5, 7, 0))
	foot_test_start_bus(pl, coord3d(3, 8, 0), [coord3d(3, 2, 0), coord3d(3, 7, 0)])
	sleep()
	sleep()

	ASSERT_EQUAL(world.generate_goods(coord(1, 2), coord(7, 7), pax, 10), 0)
}


function test_transit_by_foot_not_used_twice_consecutively()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable()

	// A --vehicle--> B --foot--> C --foot--> D --vehicle--> E
	foot_test_build_road(pl, coord3d(2, 1, 0), coord3d(2, 7, 0))
	foot_test_build_stop(pl, coord3d(2, 2, 0))
	foot_test_build_stop(pl, coord3d(2, 6, 0))
	foot_test_build_depot(pl, coord3d(2, 7, 0))
	foot_test_start_bus(pl, coord3d(2, 7, 0), [coord3d(2, 2, 0), coord3d(2, 6, 0)])

	foot_test_build_road(pl, coord3d(4, 5, 0), coord3d(4, 7, 0))
	foot_test_build_stop(pl, coord3d(4, 6, 0))

	foot_test_build_road(pl, coord3d(6, 5, 0), coord3d(6, 14, 0))
	foot_test_build_stop(pl, coord3d(6, 6, 0))
	foot_test_build_stop(pl, coord3d(6, 12, 0))
	foot_test_build_depot(pl, coord3d(6, 14, 0))
	foot_test_start_bus(pl, coord3d(6, 14, 0), [coord3d(6, 6, 0), coord3d(6, 12, 0)])
	sleep()
	sleep()

	ASSERT_EQUAL(world.generate_goods(coord(0, 2), coord(8, 12), pax, 10), 0)
}


function test_transit_by_foot_walk_cost_to_halt_prefers_nearer_halts()
{
	local pl = player_x(0)
	local pax = good_desc_x.passenger
	foot_test_enable(100, 1000, false)
	ASSERT_TRUE(settings.set_walk_cost_to_halt(true))
	ASSERT_TRUE(settings.get_walk_cost_to_halt())

	// Near and far both cover (4,0). Two two-stop schedules give them equal
	// vehicle route cost to D, leaving the origin/destination walk cost to decide.
	foot_test_build_road(pl, coord3d(2, 2, 0), coord3d(8, 2, 0))
	foot_test_build_road(pl, coord3d(8, 2, 0), coord3d(8, 12, 0))
	foot_test_build_stop(pl, coord3d(4, 2, 0))
	foot_test_build_stop(pl, coord3d(6, 2, 0))
	foot_test_build_stop(pl, coord3d(8, 10, 0))
	foot_test_build_depot(pl, coord3d(8, 12, 0))
	foot_test_start_bus(pl, coord3d(8, 12, 0), [coord3d(4, 2, 0), coord3d(8, 10, 0)])
	foot_test_start_bus(pl, coord3d(8, 12, 0), [coord3d(6, 2, 0), coord3d(8, 10, 0)])
	sleep()
	sleep()

	local near_halt = halt_x.get_halt(coord3d(4, 2, 0), pl)
	local far_halt = halt_x.get_halt(coord3d(6, 2, 0), pl)
	local halt_d = halt_x.get_halt(coord3d(8, 10, 0), pl)

	// Origin-side choice: both halts cover the origin, but Near is two tiles closer.
	ASSERT_EQUAL(world.generate_goods(coord(4, 0), coord(10, 10), pax, 10), 1)
	ASSERT_EQUAL(near_halt.waiting[0], 10)
	ASSERT_EQUAL(far_halt.waiting[0], 0)
	ASSERT_EQUAL(near_halt.get_freight_to_halt(pax, halt_d), 10)

	// Destination-side choice: D can reach both candidate destination halts with
	// equal vehicle cost, so the halt nearer to the destination tile must win.
	ASSERT_EQUAL(world.generate_goods(coord(10, 10), coord(4, 0), pax, 10), 1)
	ASSERT_EQUAL(halt_d.get_freight_to_halt(pax, near_halt), 10)
	ASSERT_EQUAL(halt_d.get_freight_to_halt(pax, far_halt), 0)
}
