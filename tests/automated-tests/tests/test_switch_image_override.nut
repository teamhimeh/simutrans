//
// Scripted integration test for fixed railway junction images.
//

function test_switch_image_override_scripted()
{
	local pl = player_x(0)
	local rail = null
	foreach (desc in way_desc_x.get_available_ways(wt_rail, st_flat)) {
		if (desc.has_switch_image() || desc.has_switch_ex_image()) {
			rail = desc
			break
		}
	}
	ASSERT_TRUE(rail != null)

	local junction = coord3d(7, 4, 0)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 4, 0), coord3d(11, 4, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(7, 1, 0), junction, rail, true), null)
	ASSERT_TRUE(dir.is_threeway(way_x(junction.x, junction.y, junction.z).get_dirs()))

	local set_image = command_x(tool_set_switch_image)
	local way = way_x(junction.x, junction.y, junction.z)
	ASSERT_EQUAL(way.get_switch_image_override(), -1)
	ASSERT_EQUAL(set_image.work(pl, junction, "0"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 0) // standard
	ASSERT_EQUAL(set_image.work(pl, junction, "3"), "") // removed legacy encoding
	ASSERT_EQUAL(set_image.work(pl, junction, "1"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 1) // NSE1
	ASSERT_EQUAL(set_image.work(pl, junction, "2"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 2) // NSE2
	ASSERT_EQUAL(set_image.work(pl, junction, "-1"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), -1)
	ASSERT_EQUAL(set_image.work(pl, junction, "2"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 2)

	// Growing the T junction to a four-way must discard the fixed choice.
	ASSERT_EQUAL(command_x.build_way(pl, junction, coord3d(7, 7, 0), rail, true), null)
	ASSERT_EQUAL(way.get_dirs(), dir.all)
	ASSERT_EQUAL(way.get_switch_image_override(), -1)
	ASSERT_EQUAL(set_image.work(pl, junction, "0"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 0)
	ASSERT_EQUAL(set_image.work(pl, junction, "2"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 2)

	// Removing an arm resolves the junction; a new override must be discarded too.
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, junction, coord3d(7, 7, 0), "" + wt_rail), null)
	ASSERT_TRUE(dir.is_threeway(way.get_dirs()))
	ASSERT_EQUAL(set_image.work(pl, junction, "2"), null)
	ASSERT_EQUAL(way.get_switch_image_override(), 2)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(7, 1, 0), junction, "" + wt_rail), null)
	ASSERT_FALSE(dir.is_threeway(way.get_dirs()))
	ASSERT_EQUAL(way.get_switch_image_override(), -1)

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 4, 0), coord3d(11, 4, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}
