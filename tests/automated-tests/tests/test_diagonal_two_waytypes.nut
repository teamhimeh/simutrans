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


// Same waytype, different descriptor: two rail tracks on disjoint diagonal bends, built via
// the straight/ctrl-pressed tool. Also exercises the merge-on-straight-build and
// split-on-neighbor-removal bookkeeping.
function test_diagonal_two_waytypes_same_waytype()
{
	local pl = player_x(0)
	local rail_a = way_desc_x("sand_track")
	local rail_b = way_desc_x("steel_sleeper_track")
	ASSERT_TRUE(rail_a != null && rail_b != null && rail_a != rail_b)

	local cx = 5
	local cy = 5

	// SE bend for rail_a at (cx,cy), two straight legs to force the exact corner tile
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy, 0), rail_a, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx + 1, cy, 0), rail_a, true), null)
	ASSERT_FALSE(tile_x(cx, cy, 0).has_two_ways())

	// NW bend for rail_b (different desc, SAME waytype) at (cx,cy), via ctrl/straight legs
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), rail_b, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx, cy - 1, 0), rail_b, true), null)

	// both legs now coexist on disjoint bends, no crossing object
	ASSERT_TRUE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_FALSE(way_x(cx, cy, 0).is_crossing())
	// the two legs' ribis union to all 4 directions (SE | NW); which one lands in weg_nr(0)
	// vs weg_nr(1) is an internal ordering detail (objlist.add() sorts by waytype, and both
	// legs share the same waytype), so query via the neighbor tiles the legs actually touch
	// rather than assuming a specific slot's ribi from the ambiguous single-way query.
	ASSERT_EQUAL(tile_x(cx, cy + 1, 0).get_way_dirs(wt_rail), dir.north)
	ASSERT_EQUAL(tile_x(cx + 1, cy, 0).get_way_dirs(wt_rail), dir.west)
	ASSERT_EQUAL(tile_x(cx - 1, cy, 0).get_way_dirs(wt_rail), dir.east)
	ASSERT_EQUAL(tile_x(cx, cy - 1, 0).get_way_dirs(wt_rail), dir.south)

	// Merge: build a straight E-W rail_a through (cx,cy) -- collapses the two legs into one
	// way covering all 4 directions.
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx + 1, cy, 0), coord3d(cx - 1, cy, 0), rail_a, true), null)
	ASSERT_FALSE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_EQUAL(tile_x(cx, cy, 0).get_way_dirs(wt_rail), dir.all)

	// Split: remove the east neighbor's rail arm -- the tile was already a single merged way
	// at this point, so this just tests ordinary ribi_rem propagation still works afterward.
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx + 1, cy, 0), coord3d(cx + 1, cy, 0), "" + wt_rail), null)
	ASSERT_FALSE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_EQUAL(tile_x(cx, cy, 0).get_way_dirs(wt_rail), dir.all & ~dir.east)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy - 1, 0), "" + wt_rail), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Split scenario that actually exercises the two-legs-still-separate collapse-to-three-way
// path (as opposed to the merge test above, which already collapsed to one way beforehand).
function test_diagonal_two_waytypes_split_to_threeway()
{
	local pl = player_x(0)
	local rail_a = way_desc_x("sand_track")
	local rail_b = way_desc_x("steel_sleeper_track")

	local cx = 5
	local cy = 5

	// SE bend for rail_a, NW bend for rail_b -- two separate legs, never merged
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy, 0), rail_a, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx + 1, cy, 0), rail_a, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), rail_b, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(cx, cy, 0), coord3d(cx, cy - 1, 0), rail_b, true), null)
	ASSERT_TRUE(tile_x(cx, cy, 0).has_two_ways())

	// sever the east arm (part of the rail_a/SE leg) -- rail_a's leg degrades from 2 bits
	// (south+east) to 1 bit (south), so it merges into rail_b's leg, producing one way with
	// a three-way ribi: north + west + south (east missing)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx + 1, cy, 0), coord3d(cx + 1, cy, 0), "" + wt_rail), null)
	ASSERT_FALSE(tile_x(cx, cy, 0).has_two_ways())
	ASSERT_EQUAL(tile_x(cx, cy, 0).get_way_dirs(wt_rail), dir.north | dir.west | dir.south)

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx, cy + 1, 0), coord3d(cx, cy - 1, 0), "" + wt_rail), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(cx - 1, cy, 0), coord3d(cx, cy, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}
