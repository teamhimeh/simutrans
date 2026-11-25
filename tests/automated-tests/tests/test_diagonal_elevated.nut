//
// Two different elevated waytypes sharing one elevated tile via disjoint diagonal bends
// (closed diagonal): an SE-bound elevated monorail and an NW-bound elevated track.
// Needs an elevated track (waytype=track, system_type=1) named "TestElevatedTrack" in the pak:
// the test pak has no elevated way of a second waytype, so the test is skipped without it.
//

function check_elevated_closed_diagonal(straight)
{
	local pl = player_x(0)
	local mono = way_desc_x.get_available_ways(wt_monorail, st_elevated)[0]
	local rail = way_desc_x("TestElevatedTrack")
	if (!rail.is_valid()) {
		print("  skipped: no elevated way of a second waytype (TestElevatedTrack) in this pak")
		return
	}
	ASSERT_TRUE(mono != null)
	ASSERT_EQUAL(rail.get_waytype(), wt_rail)

	local cx = 5
	local cy = 5

	// SE bend for the monorail at (cx,cy): (cx,cy+1) -> (cx,cy), then (cx,cy) -> (cx+1,cy)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy, 0), mono, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx + 1, cy, 0), mono, true), null)

	local t = square_x(cx, cy).get_tile_at_height(1)
	ASSERT_TRUE(t != null)
	ASSERT_FALSE(t.has_two_ways())
	ASSERT_EQUAL(t.get_way_dirs(wt_monorail), dir.south | dir.east)

	// NW bend for the elevated track at (cx,cy): (cx-1,cy) -> (cx,cy), then (cx,cy) -> (cx,cy-1)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), rail, straight), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx, cy - 1, 0), rail, straight), null)

	t = square_x(cx, cy).get_tile_at_height(1)
	ASSERT_TRUE(t.has_two_ways())
	ASSERT_EQUAL(t.get_way_dirs(wt_monorail), dir.south | dir.east)
	ASSERT_EQUAL(t.get_way_dirs(wt_rail), dir.west | dir.north)
	ASSERT_FALSE(way_x(cx, cy, 1).is_crossing())
	ASSERT_EQUAL(square_x(cx - 1, cy).get_tile_at_height(1).get_way_dirs(wt_rail), dir.east)
	ASSERT_EQUAL(square_x(cx, cy - 1).get_tile_at_height(1).get_way_dirs(wt_rail), dir.south)

	// removing the track keeps the elevated tile and the monorail
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx - 1, cy, 1), coord3d(cx, cy - 1, 1), "" + wt_rail), null)
	t = square_x(cx, cy).get_tile_at_height(1)
	ASSERT_TRUE(t != null)
	ASSERT_FALSE(t.has_two_ways())
	ASSERT_EQUAL(t.get_way_dirs(wt_monorail), dir.south | dir.east)
	ASSERT_TRUE(square_x(cx - 1, cy).get_tile_at_height(1) == null)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx, cy + 1, 1), coord3d(cx + 1, cy, 1), "" + wt_monorail), null)
	ASSERT_TRUE(square_x(cx, cy).get_tile_at_height(1) == null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_diagonal_elevated_two_waytypes_straight()
{
	check_elevated_closed_diagonal(true)
}


function test_diagonal_elevated_two_waytypes_route()
{
	check_elevated_closed_diagonal(false)
}
