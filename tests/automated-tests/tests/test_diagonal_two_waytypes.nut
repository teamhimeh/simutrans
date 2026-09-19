//
// Two different waytypes sharing one tile via disjoint diagonal bends
// (e.g. an NW-bound rail track and an SE-bound taxiway): they never touch
// at the tile center, so no crossing_t should be created for them.
//

function test_diagonal_two_waytypes_rail_and_air()
{
	local pl = player_x(0)
	local rail = way_desc_x("sand_track")
	local air_ways = way_desc_x.get_available_ways(wt_air, st_flat)
	ASSERT_TRUE(air_ways.len() > 0)
	local air = air_ways[0]

	local cx = 5
	local cy = 5

	// SE bend for rail at (cx,cy), built as two straight legs to land exactly on the
	// corner tile (a single diagonal build_way call could pick either corner):
	// (cx,cy+1) -> (cx,cy) [south leg], then (cx,cy) -> (cx+1,cy) [east leg]
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx + 1, cy, 0), rail, true), null)

	ASSERT_FALSE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_EQUAL(tile_x(cx, cy, 0).get_way_dirs(wt_rail), dir.south | dir.east)

	// NW bend for air at (cx,cy), same two-leg technique, opposite corner:
	// (cx-1,cy) -> (cx,cy) [west leg], then (cx,cy) -> (cx,cy-1) [north leg]
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), air, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx, cy - 1, 0), air, true), null)

	// both ways now coexist on the tile, on disjoint bends, without a crossing object
	ASSERT_TRUE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_EQUAL(tile_x(cx, cy, 0).get_way_dirs(wt_rail), dir.south | dir.east)
	ASSERT_EQUAL(tile_x(cx, cy, 0).get_way_dirs(wt_air), dir.west | dir.north)
	ASSERT_FALSE(way_x(cx, cy, 0).is_crossing())

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx, cy + 1, 0), coord3d(cx + 1, cy, 0), "" + wt_rail), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy - 1, 0), "" + wt_air), null)
	RESET_ALL_PLAYER_FUNDS()
}
