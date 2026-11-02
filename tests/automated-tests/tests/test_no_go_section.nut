//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

//
// Tests for demand-based skip sections (no-go-no-users sections)
//
// Route: the A-B-C bus road of test_transport.nut, plus a branch from (4,10) to the
// stop E at (9,10). With a station coverage of 2, the passenger positions used below
// are covered by exactly one stop each:
//   A (4,2)  -> (3,2)      B (4,7)  -> (3,7)
//   C (4,12) -> (3,12)     E (9,10) -> (10,10)
//

include("tests/test_transport")  // build_pax_abc_test_route()

// schedule_t::SINGLE_FREIGHT_DESTINATION
const SCHEDULE_SINGLE_FREIGHT_DESTINATION = 64

// schedule_entry_t::NO_GO_NO_USERS_SECTION_START and NO_GO_NO_USERS_SECTION_END
const NO_GO_SECTION_START  = 16777216
const NO_GO_SECTION_END    = 33554432
const NO_GO_SECTION_SINGLE = 50331648 // START | END: a one-entry section

// Waits until cond() holds, failing after max_sleeps sleep() calls.
function no_go_wait(cond, max_sleeps, what)
{
	local n = 0
	while (!cond()) {
		sleep()
		n++
		if (n > max_sleeps) {
			print("no_go_wait timed out: " + what)
			ASSERT_TRUE(false)
		}
	}
}

// Extension buildings that make the bus stops accept freight. They stand on the side
// away from the cover positions, and still leave each of those covered by one stop.
const NO_GO_FREIGHT_EXTENSION = "Bulk_StationBlg"
function no_go_freight_extension_positions()
{
	return [coord3d(5, 2, 0), coord3d(5, 7, 0), coord3d(5, 12, 0), coord3d(9, 11, 0)]
}

// Builds the route, sets time based routing for the goods and starts one vehicle with
// a schedule made of [position, flags] pairs: a bus, or with freight a coal truck
// (the stops then get a freight extension building). schedule_flags are schedule_t
// flags of the whole schedule.
function no_go_setup(entries, time_based_routing, freight = false, schedule_flags = 0)
{
	local pl = player_x(0)
	local good = freight ? good_desc_x("Kohle") : good_desc_x.passenger
	local t = {
		pl = pl
		good = good
		freight = freight
		old_tbgr = settings.get_time_based_routing_enabled(good)
	}
	ASSERT_TRUE(settings.set_time_based_routing_enabled(good, time_based_routing))
	debug.set_game_speed(5)

	build_pax_abc_test_route(pl)
	ASSERT_EQUAL(command_x(tool_build_way).work(pl, coord3d(4, 10, 0), coord3d(9, 10, 0), "cobblestone_road"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(9, 10, 0), "BusStop"), null)
	if (freight) {
		foreach (pos in no_go_freight_extension_positions()) {
			ASSERT_EQUAL(command_x(tool_build_station).work(pl, pos, NO_GO_FREIGHT_EXTENSION), null)
		}
	}

	t.a <- halt_x.get_halt(coord3d(4,  2, 0), pl)
	t.b <- halt_x.get_halt(coord3d(4,  7, 0), pl)
	t.c <- halt_x.get_halt(coord3d(4, 12, 0), pl)
	t.e <- halt_x.get_halt(coord3d(9, 10, 0), pl)

	local sched = schedule_x(wt_road, entries.map(@(en) schedule_entry_x(en[0], 0, 0, en[1])))
	sched.flags = schedule_flags
	local depot = depot_x(4, 13, 0)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x(freight ? "Kohletransporter" : "Buessig"))
	t.cnv <- depot.get_convoy_list()[0]
	t.cnv.change_schedule(pl, sched)
	depot.start_all_convoys(pl)

	// Wait for halt graph update
	sleep()
	sleep()
	return t
}

function no_go_teardown(t)
{
	debug.set_game_speed(1)
	t.cnv.destroy(t.pl)
	sleep()
	sleep()
	if (t.freight) {
		foreach (pos in no_go_freight_extension_positions()) {
			ASSERT_EQUAL(command_x(tool_remover).work(t.pl, pos), null)
		}
	}
	ASSERT_EQUAL(command_x(tool_remover).work(t.pl, coord3d(9, 10, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(t.pl, coord3d(4, 10, 0), coord3d(9, 10, 0), "" + wt_road), null)
	remove_pax_abc_test_route(t.pl)
	settings.set_time_based_routing_enabled(t.good, t.old_tbgr)
	RESET_ALL_PLAYER_FUNDS()
}

// Generates goods (passengers by default) between the cover positions of two stops
// ("a", "b", "c", "e").
function no_go_pax(from, to, amount, good = null)
{
	local pos = { a = coord(3, 2), b = coord(3, 7), c = coord(3, 12), e = coord(10, 10) }
	ASSERT_EQUAL(world.generate_goods(pos[from], pos[to], good == null ? good_desc_x.passenger : good, amount), 1)
}


// Same route without any section flag: the bus must serve every stop.
function test_no_go_section_baseline()
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), 0],
		[coord3d(4, 12, 0), 0]
	], true)

	no_go_pax("a", "c", 10)
	no_go_wait(@() t.c.arrived[0] >= 10, 3000, "arrive at C")
	ASSERT_TRUE(t.b.convoys[0] >= 1)

	no_go_teardown(t)
}


// A -> [B] -> C
function no_go_single_stop_case(time_based_routing)
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_SINGLE],
		[coord3d(4, 12, 0), 0]
	], time_based_routing)

	// Nobody uses B: B is skipped, A and C are still served.
	no_go_pax("a", "c", 10)
	no_go_wait(@() t.c.arrived[0] >= 10, 3000, "no users: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], 0)

	// Passengers at A (the stop before the section) bound for B: loaded at A, carried to B.
	no_go_pax("a", "b", 10)
	no_go_wait(@() t.b.arrived[0] >= 10, 3000, "users at A for B: arrive at B")
	ASSERT_EQUAL(t.b.convoys[0], 1)

	// Passengers waiting at B: the bus goes to B and picks them up.
	no_go_pax("b", "c", 10)
	no_go_wait(@() t.b.departed[0] >= 10, 3000, "users at B: depart from B")
	no_go_wait(@() t.c.arrived[0] >= 20, 3000, "users at B: arrive at C")

	// Demand gone again: B is skipped again.
	local b_convoys = t.b.convoys[0]
	no_go_pax("a", "c", 10)
	no_go_wait(@() t.c.arrived[0] >= 30, 3000, "no users again: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], b_convoys)

	no_go_teardown(t)
}

function test_no_go_section_single_stop()
{
	no_go_single_stop_case(true)
}

function test_no_go_section_single_stop_without_time_based_routing()
{
	no_go_single_stop_case(false)
}


// A -> [B -> C]: A is the only stop outside the section.
function test_no_go_section_two_stops()
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_START],
		[coord3d(4, 12, 0), NO_GO_SECTION_END]
	], true)

	// No users: only A is served.
	no_go_wait(@() t.a.convoys[0] >= 3, 3000, "no users: visit A three times")
	ASSERT_EQUAL(t.b.convoys[0], 0)
	ASSERT_EQUAL(t.c.convoys[0], 0)

	// Users for the section end only: the whole section is entered (B too).
	no_go_pax("a", "c", 10)
	no_go_wait(@() t.c.arrived[0] >= 10, 3000, "users at A for C: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], 1)

	// Passengers waiting at C bound for A: picked up at C, carried to A.
	no_go_pax("c", "a", 10)
	no_go_wait(@() t.c.departed[0] >= 10, 3000, "users at C: depart from C")
	no_go_wait(@() t.a.arrived[0] >= 10, 3000, "users at C: arrive at A")

	no_go_teardown(t)
}


// A -> [B -> E -> C]: the middle entry E carries no section flag of its own.
function test_no_go_section_middle_entry()
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_START],
		[coord3d(9, 10, 0), 0],
		[coord3d(4, 12, 0), NO_GO_SECTION_END]
	], true)

	// No users: the middle entry is skipped together with the rest of the section.
	no_go_wait(@() t.a.convoys[0] >= 3, 3000, "no users: visit A three times")
	ASSERT_EQUAL(t.b.convoys[0], 0)
	ASSERT_EQUAL(t.e.convoys[0], 0)
	ASSERT_EQUAL(t.c.convoys[0], 0)

	// Users for the middle entry only: the whole section is entered.
	no_go_pax("a", "e", 10)
	no_go_wait(@() t.e.arrived[0] >= 10, 3000, "users at A for E: arrive at E")
	ASSERT_EQUAL(t.b.convoys[0], 1)
	no_go_wait(@() t.c.convoys[0] >= 1, 3000, "users at A for E: stop at C")

	no_go_teardown(t)
}


// A -> W -> [B] -> C, where W is a waypoint (a road tile without a stop): the stop
// before the section is still A.
function test_no_go_section_waypoint_before_section()
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  5, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_SINGLE],
		[coord3d(4, 12, 0), 0]
	], true)

	no_go_pax("a", "c", 10)
	no_go_wait(@() t.c.arrived[0] >= 10, 3000, "no users: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], 0)

	no_go_pax("a", "b", 10)
	no_go_wait(@() t.b.arrived[0] >= 10, 3000, "users at A for B: arrive at B")

	no_go_teardown(t)
}


// A -> [B] -> C -> [E]: two sections, each decided on its own.
function test_no_go_section_two_sections()
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_SINGLE],
		[coord3d(4, 12, 0), 0],
		[coord3d(9, 10, 0), NO_GO_SECTION_SINGLE]
	], true)

	// No users: only A and C are served.
	no_go_wait(@() t.c.convoys[0] >= 2, 3000, "no users: visit C twice")
	ASSERT_EQUAL(t.b.convoys[0], 0)
	ASSERT_EQUAL(t.e.convoys[0], 0)

	// Users for B only: B is served, E still skipped.
	no_go_pax("a", "b", 10)
	no_go_wait(@() t.b.arrived[0] >= 10, 3000, "users at A for B: arrive at B")
	local c_convoys = t.c.convoys[0]
	no_go_wait(@() t.c.convoys[0] >= c_convoys + 2, 3000, "users for B: visit C twice more")
	ASSERT_EQUAL(t.e.convoys[0], 0)

	// Users at C (the stop before E) bound for E: E is served.
	no_go_pax("c", "e", 10)
	no_go_wait(@() t.e.arrived[0] >= 10, 3000, "users at C for E: arrive at E")

	no_go_teardown(t)
}


// A -> [B] -> C with a coal truck: the same four phases as for passengers.
function no_go_freight_single_stop_case(time_based_routing)
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_SINGLE],
		[coord3d(4, 12, 0), 0]
	], time_based_routing, true)

	// No freight for B: B is skipped, the coal for C is delivered.
	no_go_pax("a", "c", 10, t.good)
	no_go_wait(@() t.c.arrived[0] >= 10, 3000, "no freight for B: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], 0)

	// Coal at A (the stop before the section) bound for B: loaded at A, delivered to B.
	no_go_pax("a", "b", 10, t.good)
	no_go_wait(@() t.b.arrived[0] >= 10, 3000, "freight at A for B: arrive at B")
	ASSERT_EQUAL(t.b.convoys[0], 1)

	// Coal waiting at B bound for C: the truck goes to B and picks it up.
	no_go_pax("b", "c", 10, t.good)
	no_go_wait(@() t.b.departed[0] >= 10, 3000, "freight at B: depart from B")
	no_go_wait(@() t.c.arrived[0] >= 20, 3000, "freight at B: arrive at C")

	// No freight for B again: B is skipped again.
	local b_convoys = t.b.convoys[0]
	no_go_pax("a", "c", 10, t.good)
	no_go_wait(@() t.c.arrived[0] >= 30, 3000, "no freight again: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], b_convoys)

	no_go_teardown(t)
}

function test_no_go_section_freight_single_stop()
{
	no_go_freight_single_stop_case(true)
}

function test_no_go_section_freight_single_stop_without_time_based_routing()
{
	no_go_freight_single_stop_case(false)
}


// A -> [B] -> C with a coal truck. Freight loads for a single destination at a time
// (wagonload, the schedule's single freight destination setting). With coal for C aboard, the truck cannot load the coal waiting at B
// bound for A, so that coal must not make it stop at B. Once empty again, it can.
function test_no_go_section_freight_committed()
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), NO_GO_SECTION_SINGLE],
		[coord3d(4, 12, 0), 0]
	], true, true, SCHEDULE_SINGLE_FREIGHT_DESTINATION)

	no_go_pax("b", "a", 10, t.good)
	no_go_pax("a", "c", 10, t.good)
	no_go_wait(@() t.c.arrived[0] >= 10, 3000, "coal for C aboard: arrive at C")
	ASSERT_EQUAL(t.b.convoys[0], 0)
	ASSERT_EQUAL(t.b.waiting[0], 10)

	// Empty again after C: the truck goes to B for the coal bound for A.
	no_go_wait(@() t.b.departed[0] >= 10, 3000, "empty: load at B")
	no_go_wait(@() t.a.arrived[0] >= 10, 3000, "empty: unload at A")

	no_go_teardown(t)
}


// A -> B -> [C -> D] -> A, where D is the stop E on the branch road. B is the stop
// before the section.
function no_go_abcd_setup(freight = false)
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), 0],
		[coord3d(4, 12, 0), NO_GO_SECTION_START],
		[coord3d(9, 10, 0), NO_GO_SECTION_END]
	], true, freight)
	t.d <- t.e
	return t
}

// No goods: C and D are skipped, A and B are served.
function no_go_abcd_no_goods_case(freight)
{
	local t = no_go_abcd_setup(freight)

	no_go_wait(@() t.a.convoys[0] >= 3  &&  t.b.convoys[0] >= 3, 3000, "no goods: visit A and B three times")
	ASSERT_EQUAL(t.c.convoys[0], 0)
	ASSERT_EQUAL(t.d.convoys[0], 0)

	no_go_teardown(t)
}

function test_no_go_section_abcd_no_goods()
{
	no_go_abcd_no_goods_case(false)
}

function test_no_go_section_abcd_freight_no_goods()
{
	no_go_abcd_no_goods_case(true)
}

// Goods from B to D: loaded at B, the section is entered (C and D), unloaded at D.
function no_go_abcd_b_to_d_case(freight)
{
	local t = no_go_abcd_setup(freight)

	no_go_pax("b", "e", 10, t.good)
	no_go_wait(@() t.b.departed[0] >= 10, 3000, "goods B->D: load at B")
	no_go_wait(@() t.d.arrived[0] >= 10, 3000, "goods B->D: unload at D")
	ASSERT_EQUAL(t.c.convoys[0], 1)
	ASSERT_EQUAL(t.d.convoys[0], 1)
	ASSERT_EQUAL(t.b.waiting[0], 0)

	no_go_teardown(t)
}

function test_no_go_section_abcd_b_to_d()
{
	no_go_abcd_b_to_d_case(false)
}

function test_no_go_section_abcd_freight_b_to_d()
{
	no_go_abcd_b_to_d_case(true)
}

// Goods from D to B: the section is entered (C and D), loaded at D, unloaded at B.
function no_go_abcd_d_to_b_case(freight)
{
	local t = no_go_abcd_setup(freight)

	no_go_pax("e", "b", 10, t.good)
	no_go_wait(@() t.d.departed[0] >= 10, 3000, "goods D->B: load at D")
	no_go_wait(@() t.b.arrived[0] >= 10, 3000, "goods D->B: unload at B")
	ASSERT_EQUAL(t.c.convoys[0], 1)
	ASSERT_EQUAL(t.d.convoys[0], 1)
	ASSERT_EQUAL(t.d.waiting[0], 0)

	no_go_teardown(t)
}

function test_no_go_section_abcd_d_to_b()
{
	no_go_abcd_d_to_b_case(false)
}

function test_no_go_section_abcd_freight_d_to_b()
{
	no_go_abcd_d_to_b_case(true)
}


// A -> B -> C, no skip section, with coal at A for both B and C.
// Without the single freight destination setting the truck loads both at A; with it,
// only the coal for the nearest destination with freight (B), leaving C's waiting.
function single_freight_destination_case(single)
{
	local t = no_go_setup([
		[coord3d(4,  2, 0), 0],
		[coord3d(4,  7, 0), 0],
		[coord3d(4, 12, 0), 0]
	], true, true, single ? SCHEDULE_SINGLE_FREIGHT_DESTINATION : 0)

	no_go_pax("a", "b", 5, t.good)
	no_go_pax("a", "c", 5, t.good)
	no_go_wait(@() t.b.arrived[0] >= 5, 3000, "arrive at B")
	ASSERT_EQUAL(t.a.waiting[0], single ? 5 : 0)
	no_go_wait(@() t.c.arrived[0] >= 5, 3000, "arrive at C")

	no_go_teardown(t)
}

function test_single_freight_destination_off()
{
	single_freight_destination_case(false)
}

function test_single_freight_destination_on()
{
	single_freight_destination_case(true)
}
