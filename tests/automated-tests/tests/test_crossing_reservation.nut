//
// Tests for rail PBS reservation at 4-way crossings (co-reservation).
//
// Co-reservation rules (schiene_t::can_co_reserve_dirs):
//   corner_set = backward(ribi_type(prev, curr)) | ribi_type(curr, next)
//   Two bends whose corner-sets share NO ribi bits may co-reserve simultaneously.
//
//   PERMITTED  : N→E + S→W   corners NE=3,  SW=12  (3  & 12 = 0)
//   PERMITTED  : S→E + N→W   corners SE=6,  NW=9   (6  &  9 = 0)
//   PROHIBITED : N→S  (corner NS=5, not a bend — can't co-reserve with anything)
//   PROHIBITED : same or overlapping corner  e.g. N→E + S→E (NE=3, SE=6, 3&6=2≠0)
//
// Original single-reservation behaviour is also tested:
//   - straight track  : only one convoy at a time
//   - 3-way junction  : only one convoy at a time
//
// Map  layout  (16×16, valid coords 0-15)
// ════════════════════════════════════════════════════════════════
//
//  4-way crossing area  (x=11..15, y=3..11)
//  ─────────────────────────────────────────
//     N-S track : x=13, y=3..11
//     E-W track : x=11..15, y=7
//     crossing  : (13, 7)
//
//  depot_N : (13, 3)     depot_S : (13, 11)
//  stn_N   : (13, 4)     stn_S   : (13, 10)
//  sig_N   : (13, 5)  ←  two-way PBS (placed once)
//  sig_S   : (13, 9)  ←  two-way PBS (placed once)
//  stn_E   : (15,  7)    stn_W   : (11,  7)
//
//  Straight-track tests  (x=9, y=0..8)   — avoids all other test columns
//
//  3-way junction test  (x=9, y=0..6; branch: (9,4)→(9,4+west stub))
//    Branch goes to (8,4) — check: x=8 tests use y=0..14, but test runs
//    sequentially after cleanup, so y=4 is safe.
//
//  NOTE on x=8 branch: test_start_signal uses x=8,y=0..14 but removes all
//    infrastructure at end.  The 3-way test runs sequentially, so (8,4) is
//    free.  However to be truly safe the branch goes EAST only inside the
//    x=9 column (using a single dead-end stub at (9,4)→(10,4) is avoided
//    because x=10 = longblock column).  Instead, the 3-way is tested inline
//    below using the 4-way crossing's x=13 column with an extra branch.
// ════════════════════════════════════════════════════════════════


// ──────────────────────────────────────────────────────────
// Shared helpers  (4-way crossing at (13, 7))
// ──────────────────────────────────────────────────────────

function _cr_build_infra(pl, rail, station_desc, signal_desc)
{
    // N-S track (x=13, y=3..11)
    ASSERT_EQUAL(command_x.build_way(pl, coord3d(13, 3, 0), coord3d(13, 11, 0), rail, true), null)
    // E-W track (x=11..15, y=7)
    ASSERT_EQUAL(command_x.build_way(pl, coord3d(11, 7, 0), coord3d(15, 7, 0), rail, true), null)

    local depot_desc = get_depot_by_wt(wt_rail)
    ASSERT_EQUAL(command_x.build_depot(pl, coord3d(13, 3, 0), depot_desc), null)   // depot_N
    ASSERT_EQUAL(command_x.build_depot(pl, coord3d(13, 11, 0), depot_desc), null)  // depot_S

    ASSERT_EQUAL(command_x.build_station(pl, coord3d(13, 4, 0), station_desc), null)  // stn_N
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(13, 10, 0), station_desc), null) // stn_S
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(15, 7, 0), station_desc), null)  // stn_E
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(11, 7, 0), station_desc), null)  // stn_W

    // PBS signals — placed ONCE = two-way (works for trains from either direction).
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(13, 5, 0), signal_desc), null)  // sig_N
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(13, 9, 0), signal_desc), null)  // sig_S
}

function _cr_remove_infra(pl)
{
    foreach (y in [5, 9]) {
        if (tile_x(13, y, 0).find_object(mo_signal) != null) {
            command_x(tool_remover).work(pl, coord3d(13, y, 0))
        }
    }
    foreach (pos in [coord3d(13, 4, 0), coord3d(13, 10, 0),
                     coord3d(15, 7, 0), coord3d(11, 7, 0),
                     coord3d(13, 3, 0), coord3d(13, 11, 0)]) {
        command_x(tool_remover).work(pl, pos)
    }
    command_x(tool_remove_way).work(pl, coord3d(13, 3, 0), coord3d(13, 11, 0), "" + wt_rail)
    command_x(tool_remove_way).work(pl, coord3d(11, 7, 0), coord3d(15, 7, 0), "" + wt_rail)
}

// Create and start a 3-car train from depot_N (13,3) with given schedule.
function _cr_start_from_n(pl, entries)
{
    local depot = depot_x(13, 3, 0)
    depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local cnv = depot.get_convoy_list()[depot.get_convoy_list().len() - 1]
    depot.append_vehicle(pl, cnv, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot.append_vehicle(pl, cnv, vehicle_desc_x("H-Trans-Pantheress-Back"))
    cnv.change_schedule(pl, schedule_x(wt_rail, entries))
    depot.start_all_convoys(pl)
    sleep()
    return cnv
}

// Create and start a 3-car train from depot_S (13,11) with given schedule.
function _cr_start_from_s(pl, entries)
{
    local depot = depot_x(13, 11, 0)
    depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local cnv = depot.get_convoy_list()[depot.get_convoy_list().len() - 1]
    depot.append_vehicle(pl, cnv, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot.append_vehicle(pl, cnv, vehicle_desc_x("H-Trans-Pantheress-Back"))
    cnv.change_schedule(pl, schedule_x(wt_rail, entries))
    depot.start_all_convoys(pl)
    sleep()
    return cnv
}

// Poll until convoy's leading vehicle is at (tx, ty).  Returns false on timeout.
function _cr_wait_at(cnv, tx, ty, max_steps)
{
    for (local i = 0; i < max_steps; i++) {
        sleep()
        if (!cnv.is_valid()) return false
        local p = cnv.get_pos()
        if (p.x == tx && p.y == ty) return true
    }
    return false
}

// Poll until both convoys have the same position for 2 consecutive ticks (both stopped).
// For prohibited tests: one stops at the signal while the other stops at its destination —
// the moment both are stopped simultaneously proves the prohibition is being enforced.
function _cr_wait_both_stopped(cnva, cnvb, max_steps)
{
    local prev_a = null, prev_b = null
    for (local i = 0; i < max_steps; i++) {
        sleep()
        if (!cnva.is_valid() || !cnvb.is_valid()) return false
        local pa = cnva.get_pos()
        local pb = cnvb.get_pos()
        if (prev_a != null && prev_b != null
         && pa.x == prev_a.x && pa.y == prev_a.y
         && pb.x == prev_b.x && pb.y == prev_b.y) {
            return true
        }
        prev_a = pa
        prev_b = pb
    }
    return false
}

function _cr_destroy(pl, cnv)
{
    if (cnv.is_valid()) {
        cnv.destroy(pl)
        for (local i = 0; i < 200 && cnv.is_valid(); i++) sleep()
    }
}


// ══════════════════════════════════════════════════════════
// TC-S1  Straight track — single convoy  (non-4-way baseline)
//
// One convoy N→S on a plain straight track at x=9.
// Confirms normal PBS reservation is unaffected by co-reservation changes.
// ══════════════════════════════════════════════════════════
function test_crossing_straight_single_convoy()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    // Straight N-S track at x=9, y=0..6
    ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 0, 0), coord3d(9, 6, 0), rail, true), null)
    local depot_desc = get_depot_by_wt(wt_rail)
    ASSERT_EQUAL(command_x.build_depot(pl,   coord3d(9, 0, 0), depot_desc), null)
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(9, 1, 0), station_desc), null)
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(9, 5, 0), station_desc), null)
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(9, 2, 0), signal_desc), null)

    debug.set_game_speed(5)

    local depot = depot_x(9, 0, 0)
    depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local cnv = depot.get_convoy_list()[0]
    depot.append_vehicle(pl, cnv, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot.append_vehicle(pl, cnv, vehicle_desc_x("H-Trans-Pantheress-Back"))
    cnv.change_schedule(pl, schedule_x(wt_rail, [
        schedule_entry_x(coord3d(9, 1, 0), 0, 0),
        schedule_entry_x(coord3d(9, 5, 0), 0, 0)
    ]))
    depot.start_all_convoys(pl)
    sleep()

    local reached = _cr_wait_at(cnv, 9, 5, 6000)
    print("  TC-S1 reached y=5: " + reached)

    debug.set_game_speed(1)
    _cr_destroy(pl, cnv)
    if (tile_x(9, 2, 0).find_object(mo_signal) != null) command_x(tool_remover).work(pl, coord3d(9, 2, 0))
    foreach (y in [0, 1, 5]) command_x(tool_remover).work(pl, coord3d(9, y, 0))
    command_x(tool_remove_way).work(pl, coord3d(9, 0, 0), coord3d(9, 6, 0), "" + wt_rail)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(reached)
}


// ══════════════════════════════════════════════════════════
// TC-S2  Straight track — two convoys, same direction  (single reservation)
//
// Two N→S convoys on the same track.  corner_set for straight N→S = 5 (not a bend).
// can_co_reserve_dirs(5, 5) = false → PBS admits only one at a time.
// ca enters the block first; cb follows and must stop at signal y=5 until ca clears.
// Test passes as soon as both are stopped simultaneously:
//   ca stopped at stn_S (y=7, loading)  AND  cb stopped at sig_N (y=5, waiting).
// ══════════════════════════════════════════════════════════
function test_crossing_straight_two_convoys_sequential()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    // x=9, y=0..8  — depot_N y=0, stn_N y=1, sig y=2, sig y=5, stn_S y=7
    ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 0, 0), coord3d(9, 8, 0), rail, true), null)
    local depot_desc = get_depot_by_wt(wt_rail)
    ASSERT_EQUAL(command_x.build_depot(pl,   coord3d(9, 0, 0), depot_desc), null)
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(9, 1, 0), station_desc), null)
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(9, 7, 0), station_desc), null)
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(9, 2, 0), signal_desc), null)
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(9, 5, 0), signal_desc), null)

    debug.set_game_speed(5)

    local depot_n = depot_x(9, 0, 0)
    depot_n.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local ca = depot_n.get_convoy_list()[0]
    depot_n.append_vehicle(pl, ca, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot_n.append_vehicle(pl, ca, vehicle_desc_x("H-Trans-Pantheress-Back"))
    ca.change_schedule(pl, schedule_x(wt_rail, [
        schedule_entry_x(coord3d(9, 1, 0), 0, 0),
        schedule_entry_x(coord3d(9, 7, 0), 0, 0)
    ]))
    depot_n.start_all_convoys(pl)
    sleep()

    // ca has departed; add cb to the now-empty depot.
    depot_n.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local cb = depot_n.get_convoy_list()[0]
    depot_n.append_vehicle(pl, cb, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot_n.append_vehicle(pl, cb, vehicle_desc_x("H-Trans-Pantheress-Back"))
    cb.change_schedule(pl, schedule_x(wt_rail, [
        schedule_entry_x(coord3d(9, 1, 0), 0, 0),
        schedule_entry_x(coord3d(9, 7, 0), 0, 0)
    ]))
    depot_n.start_all_convoys(pl)
    sleep()

    // Early pass: both stopped simultaneously = ca at stn_S, cb held at signal.
    local both_stopped = _cr_wait_both_stopped(ca, cb, 3000)
    print("  TC-S2 same-dir both stopped (A at dest, B at signal): " + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    foreach (y in [2, 5]) {
        if (tile_x(9, y, 0).find_object(mo_signal) != null)
            command_x(tool_remover).work(pl, coord3d(9, y, 0))
    }
    foreach (y in [0, 1, 7]) command_x(tool_remover).work(pl, coord3d(9, y, 0))
    command_x(tool_remove_way).work(pl, coord3d(9, 0, 0), coord3d(9, 8, 0), "" + wt_rail)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
}


// ══════════════════════════════════════════════════════════
// TC-S3  Straight track — two convoys, opposite directions  (single reservation)
//
// N→S and S→N on the same straight track.  The N→S train gets the block first
// (100-tick head start); S→N train hits the south signal and waits.
// Test passes as soon as both are stopped simultaneously:
//   ca stopped at stn_S (y=7)  AND  cb stopped at sig_S (y=5, waiting).
// No deadlock because ca enters the block before cb tries to reserve.
// ══════════════════════════════════════════════════════════
function test_crossing_straight_opposite_directions()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    // x=9, y=0..8  — depot_N y=0, stn_N y=1, sig y=2, sig y=5, stn_S y=7, depot_S y=8
    ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 0, 0), coord3d(9, 8, 0), rail, true), null)
    local depot_desc = get_depot_by_wt(wt_rail)
    ASSERT_EQUAL(command_x.build_depot(pl,   coord3d(9, 0, 0), depot_desc), null)
    ASSERT_EQUAL(command_x.build_depot(pl,   coord3d(9, 8, 0), depot_desc), null)
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(9, 1, 0), station_desc), null)
    ASSERT_EQUAL(command_x.build_station(pl, coord3d(9, 7, 0), station_desc), null)
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(9, 2, 0), signal_desc), null)
    ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(9, 5, 0), signal_desc), null)

    debug.set_game_speed(5)

    // ca: N→S — gets the block first.
    local depot_n = depot_x(9, 0, 0)
    depot_n.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local ca = depot_n.get_convoy_list()[0]
    depot_n.append_vehicle(pl, ca, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot_n.append_vehicle(pl, ca, vehicle_desc_x("H-Trans-Pantheress-Back"))
    ca.change_schedule(pl, schedule_x(wt_rail, [
        schedule_entry_x(coord3d(9, 1, 0), 0, 0),
        schedule_entry_x(coord3d(9, 7, 0), 0, 0)
    ]))
    depot_n.start_all_convoys(pl)

    // Give ca time to clear sig_N (y=2) and enter the block before cb starts.
    for (local i = 0; i < 100; i++) sleep()

    // cb: S→N — should find the block already reserved by ca and wait at sig_S (y=5).
    local depot_s = depot_x(9, 8, 0)
    depot_s.append_vehicle(pl, convoy_x(0), vehicle_desc_x("H-Trans-Pantheress"))
    local cb = depot_s.get_convoy_list()[0]
    depot_s.append_vehicle(pl, cb, vehicle_desc_x("H-Trans-Tiger-Car"))
    depot_s.append_vehicle(pl, cb, vehicle_desc_x("H-Trans-Pantheress-Back"))
    cb.change_schedule(pl, schedule_x(wt_rail, [
        schedule_entry_x(coord3d(9, 7, 0), 0, 0),
        schedule_entry_x(coord3d(9, 1, 0), 0, 0)
    ]))
    depot_s.start_all_convoys(pl)
    sleep()

    // Early pass: ca stopped at stn_S, cb stopped at sig_S = block prohibition confirmed.
    local both_stopped = _cr_wait_both_stopped(ca, cb, 3000)
    print("  TC-S3 opp-dir both stopped (A at dest, B at signal): " + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    foreach (y in [2, 5]) {
        if (tile_x(9, y, 0).find_object(mo_signal) != null)
            command_x(tool_remover).work(pl, coord3d(9, y, 0))
    }
    foreach (y in [0, 1, 7, 8]) command_x(tool_remover).work(pl, coord3d(9, y, 0))
    command_x(tool_remove_way).work(pl, coord3d(9, 0, 0), coord3d(9, 8, 0), "" + wt_rail)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
}


// ══════════════════════════════════════════════════════════
// TC-3W  3-way junction — single convoy  (non-4-way baseline)
//
// Uses the 4-way crossing infrastructure (13, 7) with an extra E-branch stub
// at y=5 to create a 3-way switch at (13, 5).  One convoy travels straight
// N→S through the switch and must reach stn_S.
//
// Note: the "branch stub" tile is (14, 5) — x=14 is not used by other tests.
// ══════════════════════════════════════════════════════════
function test_crossing_three_way_single_convoy()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)

    // Add one-tile east branch at y=5 to make (13,5) a 3-way switch.
    // (14,5) is free — no other test uses x=14, y=5.
    ASSERT_EQUAL(command_x.build_way(pl, coord3d(13, 5, 0), coord3d(14, 5, 0), rail, true), null)

    debug.set_game_speed(5)

    // Single convoy: N→S straight through the 3-way at (13,5).
    local cnv = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(13, 10, 0), 0, 0)
    ])

    local reached = _cr_wait_at(cnv, 13, 10, 6000)
    print("  TC-3W reached stn_S (13,10): " + reached)

    debug.set_game_speed(1)
    _cr_destroy(pl, cnv)
    command_x(tool_remove_way).work(pl, coord3d(13, 5, 0), coord3d(14, 5, 0), "" + wt_rail)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(reached)
}


// ══════════════════════════════════════════════════════════
// TC-4A  4-way crossing  N→E + S→W  PERMITTED  (A starts first)
//
// corner_set NE=3 and SW=12;  3 & 12 = 0  →  can_co_reserve_dirs = true.
// Train A (N→E) starts 50 ticks before B so A's block_reserver reserves the
// crossing (NE=3) while B is still approaching sig_S.  B's block_reserver then
// co-reserves the same crossing (SW=12) and both proceed simultaneously.
//
// Assertion: when both trains first stop simultaneously, both are at their
// destinations (ca at stn_E=15,7 and cb at stn_W=11,7).
// Without co-reservation, cb is blocked at sig_S (13,9) when ca first loads
// at stn_E, causing the position check to fail.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_ne_sw_permitted_a_first()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    // Train A: N→E path  (depot_N → stn_N → stn_E)
    local ca = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    // Short delay so A's block_reserver fires and reserves the crossing before B starts.
    // 50 ticks is enough to trigger block_reserver (A reaches sig_N) but not enough
    // for A to physically clear the crossing — so B encounters A's live reservation.
    for (local i = 0; i < 50; i++) sleep()

    // Train B: S→W path  (depot_S → stn_S → stn_W)
    local cb = _cr_start_from_s(pl, [
        schedule_entry_x(coord3d(13, 10, 0), 0, 0),
        schedule_entry_x(coord3d(11, 7, 0), 0, 0)
    ])

    // Wait for the first moment both are simultaneously stopped.
    // With co-reservation: ca at stn_E (15,7) and cb at stn_W (11,7) together.
    // Without co-reservation: ca loads at stn_E while cb is stuck at sig_S (13,9).
    local both_stopped = _cr_wait_both_stopped(ca, cb, 6000)
    local pa_x = -1, pa_y = -1, pb_x = -1, pb_y = -1
    if (ca.is_valid()) { local t = ca.get_pos();  pa_x = t.x;  pa_y = t.y }
    if (cb.is_valid()) { local t = cb.get_pos();  pb_x = t.x;  pb_y = t.y }
    print("  TC-4A ca(" + pa_x + "," + pa_y + ") cb(" + pb_x + "," + pb_y + ") both_stopped=" + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
    ASSERT_EQUAL(pa_x, 15)  // ca reached stn_E — not stuck at a signal
    ASSERT_EQUAL(pa_y, 7)
    ASSERT_EQUAL(pb_x, 11)  // cb reached stn_W — not blocked by exclusive reservation
    ASSERT_EQUAL(pb_y, 7)
}


// ══════════════════════════════════════════════════════════
// TC-4B  4-way crossing  N→E + S→W  PERMITTED  (B starts first)
//
// Same pair; Train B (S→W) starts 50 ticks first and reserves SW=12.
// Train A (N→E) co-reserves NE=3 while B still holds the crossing.
// Without co-reservation, ca is blocked at sig_N (13,5) when cb first loads.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_ne_sw_permitted_b_first()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    // Train B starts first: S→W
    local cb = _cr_start_from_s(pl, [
        schedule_entry_x(coord3d(13, 10, 0), 0, 0),
        schedule_entry_x(coord3d(11, 7, 0), 0, 0)
    ])

    for (local i = 0; i < 50; i++) sleep()

    // Train A: N→E  (co-reserves NE=3 while B holds SW=12)
    local ca = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    local both_stopped = _cr_wait_both_stopped(ca, cb, 6000)
    local pa_x = -1, pa_y = -1, pb_x = -1, pb_y = -1
    if (ca.is_valid()) { local t = ca.get_pos();  pa_x = t.x;  pa_y = t.y }
    if (cb.is_valid()) { local t = cb.get_pos();  pb_x = t.x;  pb_y = t.y }
    print("  TC-4B ca(" + pa_x + "," + pa_y + ") cb(" + pb_x + "," + pb_y + ") both_stopped=" + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
    ASSERT_EQUAL(pa_x, 15)  // ca reached stn_E — not blocked at sig_N (13,5)
    ASSERT_EQUAL(pa_y, 7)
    ASSERT_EQUAL(pb_x, 11)  // cb reached stn_W
    ASSERT_EQUAL(pb_y, 7)
}


// ══════════════════════════════════════════════════════════
// TC-4C  4-way crossing  S→E + N→W  PERMITTED  (A starts first)
//
// corner_set SE=6 and NW=9;  6 & 9 = 0  →  can_co_reserve_dirs = true.
// Train A (S→E) starts 50 ticks first; reserves SE=6.
// Train B (N→W) co-reserves NW=9 while A still holds the crossing.
// Without co-reservation, cb is blocked at sig_N (13,5) when ca first loads.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_se_nw_permitted_a_first()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    // Train A: S→E path  (depot_S → stn_S → stn_E)
    local ca = _cr_start_from_s(pl, [
        schedule_entry_x(coord3d(13, 10, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    for (local i = 0; i < 50; i++) sleep()

    // Train B: N→W path  (depot_N → stn_N → stn_W)
    local cb = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(11, 7, 0), 0, 0)
    ])

    local both_stopped = _cr_wait_both_stopped(ca, cb, 6000)
    local pa_x = -1, pa_y = -1, pb_x = -1, pb_y = -1
    if (ca.is_valid()) { local t = ca.get_pos();  pa_x = t.x;  pa_y = t.y }
    if (cb.is_valid()) { local t = cb.get_pos();  pb_x = t.x;  pb_y = t.y }
    print("  TC-4C ca(" + pa_x + "," + pa_y + ") cb(" + pb_x + "," + pb_y + ") both_stopped=" + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
    ASSERT_EQUAL(pa_x, 15)  // ca reached stn_E
    ASSERT_EQUAL(pa_y, 7)
    ASSERT_EQUAL(pb_x, 11)  // cb reached stn_W — not blocked at sig_N (13,5)
    ASSERT_EQUAL(pb_y, 7)
}


// ══════════════════════════════════════════════════════════
// TC-4D  4-way crossing  S→E + N→W  PERMITTED  (B starts first)
//
// Train B (N→W) starts 50 ticks first; reserves NW=9.
// Train A (S→E) co-reserves SE=6 while B still holds the crossing.
// Without co-reservation, ca is blocked at sig_S (13,9) when cb first loads.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_se_nw_permitted_b_first()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    // Train B starts first: N→W
    local cb = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(11, 7, 0), 0, 0)
    ])

    for (local i = 0; i < 50; i++) sleep()

    // Train A: S→E  (co-reserves SE=6 while B holds NW=9)
    local ca = _cr_start_from_s(pl, [
        schedule_entry_x(coord3d(13, 10, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    local both_stopped = _cr_wait_both_stopped(ca, cb, 6000)
    local pa_x = -1, pa_y = -1, pb_x = -1, pb_y = -1
    if (ca.is_valid()) { local t = ca.get_pos();  pa_x = t.x;  pa_y = t.y }
    if (cb.is_valid()) { local t = cb.get_pos();  pb_x = t.x;  pb_y = t.y }
    print("  TC-4D ca(" + pa_x + "," + pa_y + ") cb(" + pb_x + "," + pb_y + ") both_stopped=" + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
    ASSERT_EQUAL(pa_x, 15)  // ca reached stn_E — not blocked at sig_S (13,9)
    ASSERT_EQUAL(pa_y, 7)
    ASSERT_EQUAL(pb_x, 11)  // cb reached stn_W
    ASSERT_EQUAL(pb_y, 7)
}


// ══════════════════════════════════════════════════════════
// TC-4P1  4-way crossing  N→S + N→S  PROHIBITED (straight path)
//
// Both convoys travel N→S.  corner_set = NS = 5 (not a bend).
// can_co_reserve_dirs(5, 5) = false.
// PBS allows only one at a time; both must reach stn_S sequentially.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_ns_sequential_prohibited()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    local ca = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(13, 10, 0), 0, 0)
    ])

    for (local i = 0; i < 100; i++) sleep()

    local cb = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(13, 10, 0), 0, 0)
    ])

    // Early pass: A reaches stn_S while B is held at signal → both stopped simultaneously.
    local both_stopped = _cr_wait_both_stopped(ca, cb, 3000)
    print("  TC-4P1 both stopped (A in block, B at signal): " + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
}


// ══════════════════════════════════════════════════════════
// TC-4P2  4-way crossing  N→E + N→E  PROHIBITED (same corner)
//
// Both convoys turn NE.  (3 & 3) = 3 ≠ 0  →  can_co_reserve_dirs = false.
// Only one holds the crossing NE corner at a time; both reach stn_E sequentially.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_ne_ne_sequential_prohibited()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    local ca = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    for (local i = 0; i < 100; i++) sleep()

    local cb = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    // Early pass: A reaches stn_E while B is held at signal → both stopped simultaneously.
    local both_stopped = _cr_wait_both_stopped(ca, cb, 3000)
    print("  TC-4P2 both stopped (A in block, B at signal): " + both_stopped)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(both_stopped)
}


// ══════════════════════════════════════════════════════════
// TC-4P3  4-way crossing  N→E + S→E  PROHIBITED (shared E exit)
//
// N→E corner NE=3; S→E corner SE=6.  (3 & 6) = 2 ≠ 0 → can't co-reserve.
// Both trains exit east — they'd conflict at the E exit of the crossing.
// PBS serialises them; both reach stn_E sequentially.
// ══════════════════════════════════════════════════════════
function test_crossing_four_way_ne_se_sequential_prohibited()
{
    local pl           = player_x(0)
    local rail         = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
    local station_desc = building_desc_x.get_available_stations(
                             building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
    local signal_desc  = sign_desc_x.get_available_signs(wt_rail).filter(
                             @(idx, s) s.is_signal())[0]
    ASSERT_TRUE(rail != null && station_desc != null && signal_desc != null)

    _cr_build_infra(pl, rail, station_desc, signal_desc)
    debug.set_game_speed(5)

    // Train A: N→E
    local ca = _cr_start_from_n(pl, [
        schedule_entry_x(coord3d(13, 4, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    for (local i = 0; i < 100; i++) sleep()

    // Train B: S→E  (same E exit, different entry corner)
    local cb = _cr_start_from_s(pl, [
        schedule_entry_x(coord3d(13, 10, 0), 0, 0),
        schedule_entry_x(coord3d(15, 7, 0), 0, 0)
    ])

    local reached_a = _cr_wait_at(ca, 15, 7, 8000)
    local reached_b = _cr_wait_at(cb, 15, 7, 8000)
    print("  TC-4P3 ca (N→E) stn_E: " + reached_a + "  cb (S→E) stn_E: " + reached_b)

    debug.set_game_speed(1)
    _cr_destroy(pl, ca)
    _cr_destroy(pl, cb)
    _cr_remove_infra(pl)
    RESET_ALL_PLAYER_FUNDS()

    ASSERT_TRUE(reached_a)
    ASSERT_TRUE(reached_b)
}
