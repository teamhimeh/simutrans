//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for bridge pillar removal tool (TOOL_REMOVE_PILLAR)
//


function count_pillars(pos)
{
	local count = 0
	foreach (obj in tile_x(pos.x, pos.y, pos.z).get_objects()) {
		if (obj.get_type() == mo_pillar) {
			count++
		}
	}
	return count
}


function build_pillared_road_bridge(pl, start_pos, end_pos)
{
	local bridge = bridge_desc_x("ModernRoad")
	ASSERT_TRUE(bridge != null)
	ASSERT_EQUAL(command_x.build_bridge(pl, start_pos, end_pos, bridge), null)
}


// Single-click removes exactly one pillar; bridge and way remain intact.
// A second click on the now-empty tile returns "" (normal end, not an error).
function test_remove_pillar_tool_basic()
{
	local pl = player_x(0)
	local pillar_remover = command_x(tool_remove_pillar)
	local road = way_desc_x("cobblestone_road")
	local target = coord3d(4, 4, 0)
	local other = coord3d(4, 6, 0)

	ASSERT_TRUE(road != null)
	build_pillared_road_bridge(pl, coord3d(4, 1, 0), coord3d(4, 7, 0))
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), road, true), null)

	ASSERT_TRUE(tile_x(target.x, target.y, target.z).find_object(mo_pillar) != null)
	ASSERT_TRUE(tile_x(other.x, other.y, other.z).find_object(mo_pillar) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, 1).find_object(mo_bridge) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, 1).get_way(wt_road) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, target.z).get_way(wt_road) != null)

	// Remove the pillar — bridge and ground way must be unaffected.
	ASSERT_EQUAL(pillar_remover.work(pl, target), null)

	ASSERT_EQUAL(tile_x(target.x, target.y, target.z).find_object(mo_pillar), null)
	ASSERT_TRUE(tile_x(other.x, other.y, other.z).find_object(mo_pillar) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, 1).find_object(mo_bridge) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, 1).get_way(wt_road) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, target.z).get_way(wt_road) != null)

	// Second click on a tile with no pillar must return "" (not an error string)
	// so that the area-removal loop in do_work terminates cleanly.
	ASSERT_EQUAL(pillar_remover.work(pl, target), "")
	ASSERT_TRUE(tile_x(other.x, other.y, other.z).find_object(mo_pillar) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, 1).find_object(mo_bridge) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, 1).get_way(wt_road) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, target.z).get_way(wt_road) != null)
}


// generic remover removes pillars before signals and powerlines.
function test_remover_pillar_priority()
{
	local pl = player_x(0)
	local remover = command_x(tool_remover)
	local rail = way_desc_x("sand_track")
	local powerline = way_desc_x.get_available_ways(wt_power, st_flat)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]
	local target = coord3d(4, 4, 0)
	local power_target = coord3d(10, 4, 0)

	ASSERT_TRUE(rail != null)
	ASSERT_TRUE(powerline != null)
	ASSERT_TRUE(signal != null)

	build_pillared_road_bridge(pl, coord3d(4, 1, 0), coord3d(4, 7, 0))
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(12, 4, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, target, signal), null)
	// A powerline and a pillared bridge cannot be built over each other, so use a
	// separate crossing to verify the signal -> powerline part of the order.
	ASSERT_EQUAL(command_x.build_sign_at(pl, power_target, signal), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(10, 2, 0), coord3d(10, 6, 0), powerline, true), null)

	ASSERT_TRUE(tile_x(target.x, target.y, target.z).find_object(mo_pillar) != null)
	ASSERT_TRUE(tile_x(target.x, target.y, target.z).find_object(mo_signal) != null)
	ASSERT_TRUE(tile_x(power_target.x, power_target.y, power_target.z).find_object(mo_signal) != null)
	ASSERT_TRUE(tile_x(power_target.x, power_target.y, power_target.z).find_object(mo_powerline) != null)

	// First removal at target removes pillar (highest priority), not signal.
	ASSERT_EQUAL(remover.work(pl, target), null)
	ASSERT_EQUAL(tile_x(target.x, target.y, target.z).find_object(mo_pillar), null)
	ASSERT_TRUE(tile_x(target.x, target.y, target.z).find_object(mo_signal) != null)

	// Second removal removes signal.
	ASSERT_EQUAL(remover.work(pl, target), null)
	ASSERT_EQUAL(tile_x(target.x, target.y, target.z).find_object(mo_signal), null)
	ASSERT_TRUE(tile_x(target.x, target.y, target.z).get_way(wt_rail) != null)

	// At power_target: signal is removed before powerline.
	ASSERT_EQUAL(remover.work(pl, power_target), null)
	ASSERT_EQUAL(tile_x(power_target.x, power_target.y, power_target.z).find_object(mo_signal), null)
	ASSERT_TRUE(tile_x(power_target.x, power_target.y, power_target.z).find_object(mo_powerline) != null)

	ASSERT_EQUAL(remover.work(pl, power_target), null)
	ASSERT_EQUAL(tile_x(power_target.x, power_target.y, power_target.z).find_object(mo_powerline), null)
	ASSERT_TRUE(tile_x(power_target.x, power_target.y, power_target.z).get_way(wt_rail) != null)
}


// Shift+Ctrl area removal clears all pillars in the rectangle and returns ""
// (not an error), even when tiles within the area have multiple pillar layers
// from stacked bridges.
function test_remove_pillar_tool_shift_area()
{
	local pl = player_x(0)
	local pillar_remover = command_x(tool_remove_pillar)
	local setslope = command_x.set_slope

	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.south), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 7, 0), slope.north), null)
	build_pillared_road_bridge(pl, coord3d(3, 2, 0), coord3d(3, 7, 0))

	ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 0), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.south), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 8, 0), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 8, 1), slope.north), null)
	build_pillared_road_bridge(pl, coord3d(3, 1, 1), coord3d(3, 8, 1))

	ASSERT_TRUE(count_pillars(coord3d(3, 4, 0)) >= 2)
	ASSERT_TRUE(count_pillars(coord3d(3, 2, 0)) >= 1)
	ASSERT_TRUE(count_pillars(coord3d(3, 6, 0)) >= 2)
	ASSERT_TRUE(tile_x(3, 4, 2).find_object(mo_bridge) != null)

	// Shift + Ctrl enters box processing and removes every pillar in the area.
	pillar_remover.set_flags(3)
	ASSERT_EQUAL(pillar_remover.work(pl, coord3d(3, 2, 0), coord3d(3, 4, 0), ""), "")

	ASSERT_EQUAL(count_pillars(coord3d(3, 4, 0)), 0)
	ASSERT_EQUAL(count_pillars(coord3d(3, 2, 0)), 0)
	ASSERT_TRUE(count_pillars(coord3d(3, 6, 0)) >= 2)
	ASSERT_TRUE(tile_x(3, 4, 2).find_object(mo_bridge) != null)
	ASSERT_TRUE(tile_x(3, 4, 2).get_way(wt_road) != null)
	ASSERT_TRUE(tile_x(3, 6, 2).find_object(mo_bridge) != null)
}
