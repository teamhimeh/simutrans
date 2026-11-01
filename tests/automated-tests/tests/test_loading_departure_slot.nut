//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

function loading_slot_wait_until(ticks)
{
	while (world.get_time().ticks < ticks) { sleep() }
}

function loading_slot_wait_for_stop(cnv)
{
	local deadline = world.get_time().ticks + world.get_time().ticks_per_month
	while (!cnv.is_loading()) {
		ASSERT_LESS(world.get_time().ticks, deadline)
		sleep()
	}
}

// Separate roads let both buses load without blocking each other. The adjacent
// stops form one halt; both lines use the same departure slot group and stop index.
function loading_slot_fixture(load, wait = 0, spacing = 4, shift = 0, tolerance = 0, timed = true, extra_flags = 0)
{
	local pl = player_x(0)
	// No previous journeys are needed to establish the test passenger route.
	ASSERT_TRUE(settings.set_time_based_routing_enabled(good_desc_x.passenger, false))
	local convoys = []
	local depots = []
	local leader = null
	for (local x = 4; x <= 5; x++) {
		ASSERT_EQUAL(command_x(tool_build_way).work(pl, coord3d(x, 2, 0), coord3d(x, 9, 0), "cobblestone_road"), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(x, 2, 0), "BusStop"), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(x, 7, 0), "BusStop"), null)
		ASSERT_EQUAL(command_x.build_depot(pl, coord3d(x, 9, 0), building_desc_x("CarDepot")), null)
		ASSERT_TRUE(pl.create_line(wt_road))
		local lines = pl.get_line_list()
		local line = lines[lines.get_count() - 1]
		local sched = schedule_x(wt_road, [
			schedule_entry_x(coord3d(x, 7, 0), load, wait),
			// Hold at the destination so the first departure remains observable.
			schedule_entry_x(coord3d(x, 2, 0), 100, 0),
		])
		sched.entries[0].flags = (timed ? 16 : 0) | extra_flags // WAIT_FOR_TIME
		sched.entries[0].spacing = spacing
		sched.entries[0].spacing_shift = shift
		sched.entries[0].delay_tolerance = tolerance
		sched.departure_slot_group_id = leader == null ? line : leader
		line.change_schedule(pl, sched)
		if (leader == null) { leader = line }
		local depot = depot_x(x, 9, 0)
		depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
		local cnv = depot.get_convoy_list()[0]
		cnv.set_line(pl, line)
		ASSERT_EQUAL(cnv.get_schedule().entries.len(), 2)
		ASSERT_EQUAL(cnv.get_schedule().entries[0].load, load)
		convoys.append(cnv)
		depots.append(depot)
	}
	ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 7, 0), pl).id, halt_x.get_halt(coord3d(5, 7, 0), pl).id)
	debug.set_game_speed(100)
	return { convoys = convoys, depots = depots }
}

function test_loading_departure_slot_after_long_wait()
{
	local f = loading_slot_fixture(50)
	local month = world.get_time().ticks_per_month
	local interval = month / 4
	f.depots[0].start_all_convoys(player_x(0))
	loading_slot_wait_for_stop(f.convoys[0])
	// Arrival times are more than one slot apart, so the old code assigns
	// different obsolete slots and lets both buses leave immediately when loaded.
	loading_slot_wait_until(world.get_time().ticks + interval + 8192)
	f.depots[1].start_all_convoys(player_x(0))
	loading_slot_wait_for_stop(f.convoys[1])
	local release = (world.get_time().ticks / month + 2) * month + interval / 2
	loading_slot_wait_until(release)
	print("Loading slot release at " + world.get_time().ticks)
	print("Destination halt " + halt_x.get_halt(coord3d(4, 2, 0), player_x(0)).id + ", source halt " + halt_x.get_halt(coord3d(4, 7, 0), player_x(0)).id)
	foreach (cnv in f.convoys) {
		ASSERT_TRUE(cnv.is_loading())
		ASSERT_EQUAL(cnv.get_loading_level(), 0)
	}
	ASSERT_EQUAL(world.generate_goods(coord(3, 7), coord(3, 2), good_desc_x.passenger, 200), 1)
	print("Passengers generated")
	local deadline = release + interval / 4
	while (f.convoys[0].get_loading_level() < 50 || f.convoys[1].get_loading_level() < 50) {
		ASSERT_LESS(world.get_time().ticks, deadline)
		sleep()
	}
	local next_slot = (release / interval + 1) * interval
	print("Both buses loaded at " + world.get_time().ticks + "; next slot " + next_slot)
	loading_slot_wait_until(next_slot - 4096)
	foreach (cnv in f.convoys) {
		ASSERT_EQUAL(cnv.get_schedule().current, 0)
		ASSERT_TRUE(cnv.is_loading())
	}
	// The first booking must stay fixed while waiting, and the other bus must
	// receive the next slot, even though both were loaded at nearly the same time.
	loading_slot_wait_until(next_slot + interval / 2)
	local departed = 0
	foreach (cnv in f.convoys) {
		if (cnv.get_schedule().current == 1) { departed++ }
	}
	ASSERT_EQUAL(departed, 1)
	loading_slot_wait_until(next_slot + interval + interval / 2)
	foreach (cnv in f.convoys) { ASSERT_EQUAL(cnv.get_schedule().current, 1) }
}

function test_loading_departure_slot_delay_tolerance()
{
	// A shift of 90 and tolerance of 90 are each 1/16 month (divisor 1440).
	local f = loading_slot_fixture(50, 0, 4, 90, 90)
	local month = world.get_time().ticks_per_month
	f.depots[0].start_all_convoys(player_x(0))
	loading_slot_wait_for_stop(f.convoys[0])
	local slot = (world.get_time().ticks / month + 1) * month + month / 16
	local release = slot + month / 32
	loading_slot_wait_until(release)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 0)
	ASSERT_EQUAL(world.generate_goods(coord(3, 7), coord(3, 2), good_desc_x.passenger, 100), 1)
	// The just-missed slot is still within tolerance: no need to wait 1/4 month.
	loading_slot_wait_until(release + month / 64)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 1)
}

function loading_slot_timeout_case(load, flags)
{
	local f = loading_slot_fixture(load, 2, 4, 90, 0, true, flags)
	local month = world.get_time().ticks_per_month
	f.depots[0].start_all_convoys(player_x(0))
	loading_slot_wait_for_stop(f.convoys[0])
	local arrival = world.get_time().ticks
	local timeout = arrival + month / 2
	loading_slot_wait_until(timeout - 4096)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 0)
	local next_slot = ((timeout - month / 16) / (month / 4) + 1) * (month / 4) + month / 16
	loading_slot_wait_until(next_slot - 8192)
	ASSERT_EQUAL(f.convoys[0].get_loading_level(), 0)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 0)
	loading_slot_wait_until(next_slot + month / 8)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 1)
}

function test_loading_departure_slot_wait_timeout()
{
	loading_slot_timeout_case(50, 0)
}

function test_departure_slot_allowance_timeout()
{
	// WAIT_FOR_OTHER_CONVOY, with no loading condition to mask the bug.
	loading_slot_timeout_case(0, 1 << 20)
}

function test_departure_slot_coupling_timeout()
{
	// WAIT_FOR_COUPLING | WAIT_COUPLING_DONE, again with minimum loading zero.
	loading_slot_timeout_case(0, 1 | (1 << 10))
}

function test_loading_departure_slot_without_loading_limit()
{
	local f = loading_slot_fixture(0, 0, 1)
	local month = world.get_time().ticks_per_month
	f.depots[0].start_all_convoys(player_x(0))
	loading_slot_wait_for_stop(f.convoys[0])
	local next_slot = (world.get_time().ticks / month + 1) * month
	loading_slot_wait_until(next_slot - 8192)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 0)
	loading_slot_wait_until(next_slot + month / 8)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 1)
}

function test_loading_departure_slot_without_time_wait()
{
	local f = loading_slot_fixture(50, 0, 1, 0, 0, false)
	local month = world.get_time().ticks_per_month
	f.depots[0].start_all_convoys(player_x(0))
	loading_slot_wait_for_stop(f.convoys[0])
	loading_slot_wait_until(world.get_time().ticks + month / 4)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 0)
	ASSERT_EQUAL(world.generate_goods(coord(3, 7), coord(3, 2), good_desc_x.passenger, 100), 1)
	loading_slot_wait_until(world.get_time().ticks + month / 64)
	print("Untimed bus loading " + f.convoys[0].get_loading_level() + ", current " + f.convoys[0].get_schedule().current)
	ASSERT_EQUAL(f.convoys[0].get_schedule().current, 1)
}
