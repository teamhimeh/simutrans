// Same waytype, SAME descriptor: two rail tracks using the identical desc on disjoint
// diagonal bends, built via the straight/ctrl-pressed tool. The player wants two independent,
// non-connected tracks here rather than a merged junction, even though nothing about the
// descriptor distinguishes them.
function test_diagonal_two_waytypes_same_desc()
{
	local pl = player_x(0)
	local rail = way_desc_x("sand_track")

	local cx = 5
	local cy = 5

	// SE bend, two straight legs to force the exact corner tile
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx + 1, cy, 0), rail, true), null)
	ASSERT_FALSE(tile_x(cx, cy, 0).has_two_ways())

	// NW bend, SAME descriptor, via ctrl/straight legs
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx, cy - 1, 0), rail, true), null)

	// both legs coexist on disjoint bends, no crossing object, despite the identical descriptor
	ASSERT_TRUE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_FALSE(way_x(cx, cy, 0).is_crossing())
	ASSERT_EQUAL(tile_x(cx, cy + 1, 0).get_way_dirs(wt_rail), dir.north)
	ASSERT_EQUAL(tile_x(cx + 1, cy, 0).get_way_dirs(wt_rail), dir.west)
	ASSERT_EQUAL(tile_x(cx - 1, cy, 0).get_way_dirs(wt_rail), dir.east)
	ASSERT_EQUAL(tile_x(cx, cy - 1, 0).get_way_dirs(wt_rail), dir.south)

	// clean up -- the tile is still split into two separate legs (SE: south+east,
	// NW: north+west), so each removal call must match one leg's own two endpoints
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx, cy + 1, 0), coord3d(cx + 1, cy, 0), "" + wt_rail), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy - 1, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}
