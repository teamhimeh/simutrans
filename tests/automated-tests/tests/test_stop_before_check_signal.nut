//
// Tests for "stop before check" flag on rail signals.
// (simple signal, longblock signal, choose signal)
//
// Behavioral summary of the flag:
//   stop_before_check == false (default):
//     The convoy reserves the block ahead while still moving;
//     the signal may turn green before the convoy arrives.
//   stop_before_check == true:
//     signal_clear is forced false and restart_speed is set to -1
//     until the convoy has fully stopped (is_waiting == true) at the signal;
//     only then does the signal check whether the block ahead is clear.
//


// ─── Test 1: default value ────────────────────────────────────────────────────
// Verify that stop_before_check is false on freshly built signals.
function test_stop_before_check_default_false()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]

	local signal_desc = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_signal())[0]
	local lb_desc     = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_longblock_signal())[0]
	local ch_desc     = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_choose_sign())[0]

	ASSERT_TRUE(signal_desc != null)
	ASSERT_TRUE(lb_desc     != null)
	ASSERT_TRUE(ch_desc     != null)

	// Build short tracks and place one signal of each type.
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 0, 0), coord3d(2, 6, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 1, 0), signal_desc), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), lb_desc),     null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 5, 0), ch_desc),     null)

	local sig_simple = tile_x(2, 1, 0).find_object(mo_signal)
	local sig_lb     = tile_x(2, 3, 0).find_object(mo_signal)
	local sig_ch     = tile_x(2, 5, 0).find_object(mo_signal)

	ASSERT_TRUE(sig_simple != null)
	ASSERT_TRUE(sig_lb     != null)
	ASSERT_TRUE(sig_ch     != null)

	// Default flag must be false.
	ASSERT_FALSE(sig_simple.is_stop_before_check())
	ASSERT_FALSE(sig_lb.is_stop_before_check())
	ASSERT_FALSE(sig_ch.is_stop_before_check())

	// Clean up.
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 1, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 3, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 5, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(
	                 pl, coord3d(2, 0, 0), coord3d(2, 6, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}


// ─── Test 2: set / get round-trip ─────────────────────────────────────────────
// Verify that setting the flag and reading it back works for all three types.
// Note: command_x.set_stop_before_check uses call_tool_init internally,
//       which returns true (not null) on success; do not assert the return value.
function test_stop_before_check_set_get()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]

	local signal_desc = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_signal())[0]
	local lb_desc     = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_longblock_signal())[0]
	local ch_desc     = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_choose_sign())[0]

	ASSERT_TRUE(signal_desc != null)
	ASSERT_TRUE(lb_desc     != null)
	ASSERT_TRUE(ch_desc     != null)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 0, 0), coord3d(2, 6, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 1, 0), signal_desc), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), lb_desc),     null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 5, 0), ch_desc),     null)

	local positions = [ coord3d(2, 1, 0), coord3d(2, 3, 0), coord3d(2, 5, 0) ]

	foreach (pos in positions) {
		local sig = tile_x(pos.x, pos.y, pos.z).find_object(mo_signal)
		ASSERT_TRUE(sig != null)
		ASSERT_FALSE(sig.is_stop_before_check())

		// Set to true – verify via is_stop_before_check(), not the return value.
		command_x.set_stop_before_check(pl, pos, 1)
		ASSERT_TRUE(sig.is_stop_before_check())

		// Set back to false.
		command_x.set_stop_before_check(pl, pos, 0)
		ASSERT_FALSE(sig.is_stop_before_check())
	}

	// Clean up.
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 1, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 3, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 5, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(
	                 pl, coord3d(2, 0, 0), coord3d(2, 6, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}


// ─── Shared helpers for convoy-based tests ────────────────────────────────────

// Build infrastructure common to convoy tests:
//   Track     : (4,0) – (4,14)
//   Depot     : (4,0)
//   Station A : (4,2)
//   Station B : (4,12)
//   Signal    : (4,7)   (caller supplies the descriptor)
// Returns the built signal object.
function _build_convoy_test_infra(pl, rail, signal_desc)
{
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 0, 0), coord3d(4, 14, 0), rail, true), null)

	local station_desc = building_desc_x.get_available_stations(
	                         building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
	ASSERT_TRUE(station_desc != null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4,  2, 0), station_desc), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 12, 0), station_desc), null)

	local depot_desc = get_depot_by_wt(wt_rail)
	ASSERT_TRUE(depot_desc != null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(4, 0, 0), depot_desc), null)

	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(4, 7, 0), signal_desc), null)
	local sig = tile_x(4, 7, 0).find_object(mo_signal)
	ASSERT_TRUE(sig != null)
	return sig
}

// Start a single-vehicle convoy from the depot.
// Returns the convoy object after it has left the depot.
function _start_test_convoy(pl)
{
	local depot = depot_x(4, 0, 0)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
	local cnv = depot.get_convoy_list()[0]
	cnv.change_schedule(pl, schedule_x(wt_rail, [
		schedule_entry_x(coord3d(4,  2, 0), 0, 0),
		schedule_entry_x(coord3d(4, 12, 0), 0, 0)
	]))
	depot.start_all_convoys(pl)
	sleep()
	return cnv
}

// Tear down everything built by _build_convoy_test_infra.
function _remove_convoy_test_infra(pl)
{
	local sig = tile_x(4, 7, 0).find_object(mo_signal)
	if (sig != null) {
		command_x(tool_remover).work(pl, coord3d(4, 7, 0))
	}
	command_x(tool_remover).work(pl, coord3d(4,  2, 0))
	command_x(tool_remover).work(pl, coord3d(4, 12, 0))
	command_x(tool_remover).work(pl, coord3d(4,  0, 0))
	command_x(tool_remove_way).work(
	    pl, coord3d(4, 0, 0), coord3d(4, 14, 0), "" + wt_rail)
}


// ─── Test 3: simple signal – convoy must stop (is_waiting) at signal ──────────
//
// With stop_before_check=true the signal is held red until the convoy is fully
// waiting (is_waiting()==true); only then does the block check run.
// We verify cnv.is_waiting() while cnv.get_pos().y == 7 (signal tile).
function test_stop_before_check_simple_signal_convoy_stops()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal_desc = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_signal())[0]
	ASSERT_TRUE(rail        != null)
	ASSERT_TRUE(signal_desc != null)

	local sig = _build_convoy_test_infra(pl, rail, signal_desc)

	// Enable stop_before_check.
	command_x.set_stop_before_check(pl, coord3d(4, 7, 0), 1)
	ASSERT_TRUE(sig.is_stop_before_check())
	ASSERT_EQUAL(sig.get_state(), state_red)

	local cnv = _start_test_convoy(pl)

	debug.set_game_speed(5)

	local stopped_at_signal = false
	local convoy_passed     = false
	local max_steps = 3000

	for (local i = 0; i < max_steps; i++) {
		sleep()
		if (!cnv.is_valid()) break

		local pos = cnv.get_pos()

		// Convoy must enter waiting state ON the signal tile.
		if (pos.y == 7 && cnv.is_waiting()) {
			stopped_at_signal = true
			// Signal must still be red while the convoy is waiting.
			ASSERT_EQUAL(sig.get_state(), state_red)
		}

		// After having stopped, the convoy must eventually proceed past y==7.
		if (stopped_at_signal && pos.y > 7) {
			convoy_passed = true
			break
		}
	}

	print("  stopped_at_signal: " + stopped_at_signal)
	print("  convoy_passed:     " + convoy_passed)

	ASSERT_TRUE(stopped_at_signal)
	ASSERT_TRUE(convoy_passed)

	cnv.destroy(pl)
	sleep()
	debug.set_game_speed(1)
	_remove_convoy_test_infra(pl)
	RESET_ALL_PLAYER_FUNDS()
}


// ─── Test 4: longblock signal – convoy must stop (is_waiting) at signal ───────
function test_stop_before_check_longblock_signal_convoy_stops()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local lb_desc = sign_desc_x.get_available_signs(wt_rail).filter(
	                    @(idx, s) s.is_longblock_signal())[0]
	ASSERT_TRUE(rail    != null)
	ASSERT_TRUE(lb_desc != null)

	local sig = _build_convoy_test_infra(pl, rail, lb_desc)

	command_x.set_stop_before_check(pl, coord3d(4, 7, 0), 1)
	ASSERT_TRUE(sig.is_stop_before_check())
	ASSERT_EQUAL(sig.get_state(), state_red)

	local cnv = _start_test_convoy(pl)

	debug.set_game_speed(5)

	local stopped_at_signal = false
	local convoy_passed     = false
	local max_steps = 3000

	for (local i = 0; i < max_steps; i++) {
		sleep()
		if (!cnv.is_valid()) break

		local pos = cnv.get_pos()

		if (pos.y == 7 && cnv.is_waiting()) {
			stopped_at_signal = true
			ASSERT_EQUAL(sig.get_state(), state_red)
		}

		if (stopped_at_signal && pos.y > 7) {
			convoy_passed = true
			break
		}
	}

	print("  stopped_at_signal: " + stopped_at_signal)
	print("  convoy_passed:     " + convoy_passed)

	ASSERT_TRUE(stopped_at_signal)
	ASSERT_TRUE(convoy_passed)

	cnv.destroy(pl)
	sleep()
	debug.set_game_speed(1)
	_remove_convoy_test_infra(pl)
	RESET_ALL_PLAYER_FUNDS()
}


// ─── Test 5: choose signal – convoy must stop (is_waiting) at signal ──────────
function test_stop_before_check_choose_signal_convoy_stops()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local ch_desc = sign_desc_x.get_available_signs(wt_rail).filter(
	                    @(idx, s) s.is_choose_sign())[0]
	ASSERT_TRUE(rail    != null)
	ASSERT_TRUE(ch_desc != null)

	local sig = _build_convoy_test_infra(pl, rail, ch_desc)

	command_x.set_stop_before_check(pl, coord3d(4, 7, 0), 1)
	ASSERT_TRUE(sig.is_stop_before_check())
	ASSERT_EQUAL(sig.get_state(), state_red)

	local cnv = _start_test_convoy(pl)

	debug.set_game_speed(5)

	local stopped_at_signal = false
	local convoy_passed     = false
	local max_steps = 3000

	for (local i = 0; i < max_steps; i++) {
		sleep()
		if (!cnv.is_valid()) break

		local pos = cnv.get_pos()

		if (pos.y == 7 && cnv.is_waiting()) {
			stopped_at_signal = true
			ASSERT_EQUAL(sig.get_state(), state_red)
		}

		if (stopped_at_signal && pos.y > 7) {
			convoy_passed = true
			break
		}
	}

	print("  stopped_at_signal: " + stopped_at_signal)
	print("  convoy_passed:     " + convoy_passed)

	ASSERT_TRUE(stopped_at_signal)
	ASSERT_TRUE(convoy_passed)

	cnv.destroy(pl)
	sleep()
	debug.set_game_speed(1)
	_remove_convoy_test_infra(pl)
	RESET_ALL_PLAYER_FUNDS()
}


// ─── Test 6: without the flag, convoy does NOT wait at signal ─────────────────
// Complement test: with stop_before_check=false (default) and an empty block
// ahead, the convoy should pass the signal without ever entering waiting state.
function test_stop_before_check_false_convoy_does_not_stop()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal_desc = sign_desc_x.get_available_signs(wt_rail).filter(
	                        @(idx, s) s.is_signal())[0]
	ASSERT_TRUE(rail        != null)
	ASSERT_TRUE(signal_desc != null)

	local sig = _build_convoy_test_infra(pl, rail, signal_desc)

	// Leave stop_before_check at its default (false).
	ASSERT_FALSE(sig.is_stop_before_check())

	local cnv = _start_test_convoy(pl)

	debug.set_game_speed(5)

	local waited_at_signal = false
	local convoy_passed    = false
	local max_steps = 3000

	for (local i = 0; i < max_steps; i++) {
		sleep()
		if (!cnv.is_valid()) break

		local pos = cnv.get_pos()

		// If the convoy is waiting on the signal tile, record it.
		if (pos.y == 7 && cnv.is_waiting()) {
			waited_at_signal = true
		}

		if (pos.y > 7) {
			convoy_passed = true
			break
		}
	}

	print("  waited_at_signal (expected false): " + waited_at_signal)
	print("  convoy_passed:                     " + convoy_passed)

	// Without the flag the convoy should pass without entering waiting state.
	ASSERT_FALSE(waited_at_signal)
	ASSERT_TRUE(convoy_passed)

	cnv.destroy(pl)
	sleep()
	debug.set_game_speed(1)
	_remove_convoy_test_infra(pl)
	RESET_ALL_PLAYER_FUNDS()
}
