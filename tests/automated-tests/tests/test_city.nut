//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

//
// Tests for adding, removing and deleting cities
//


function test_city_add_invalid()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(-1, -1, 0)), "")
}


function test_city_add_cannot_afford()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(0), coord3d(7, 8, 0)), "Insufficient funds!")

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_by_public_player()
{
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), null)

		ASSERT_TRUE(tile_x(7, 8, 0).find_object(mo_building) != null)
		ASSERT_TRUE(tile_x(7, 9, 0).get_way(wt_road) != null)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(7, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(7, 9, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_on_existing_townhall()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), null)

	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(7, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(7, 9, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_near_map_border()
{
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(0, 15, 0)), null)

		ASSERT_TRUE(tile_x(0, 14, 0).find_object(mo_building) != null)
		ASSERT_TRUE(tile_x(0, 15, 0).get_way(wt_road) != null)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(0, 14, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(0, 15, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}

function test_city_change_size_invalid_params()
{
	local pl = player_x(0)

	// not a city
	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), "100"), "")
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(1, 1, 0)), null)

	// invalid pos
	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(-1,-1,-1), "100"), "")
	}

	// Null default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), null), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// Empty default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), ""), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// Wrong default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), "bla"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(1, 1, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(1, 2, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_change_size_to_minimum()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(1, 1, 0)), null)
	ASSERT_EQUAL(city_x(1, 1).get_citizens()[0], 126)

	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0),  coord3d(1, 1, 0), "-100"), null)
		ASSERT_EQUAL(city_x(1, 1).get_citizens()[0], 126)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(1, 1, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(1, 2, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


// The townhall is in the corner (0, 0): the tiles on its far side are outside of the map.
// When its road is gone and the city grows, the townhall is rebuilt and the search for the old road
// must not look at tiles outside of the map.
function test_city_townhall_rebuild_without_road_at_map_border()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(0, 0, 0)), null)
	ASSERT_TRUE(tile_x(0, 0, 0).find_object(mo_building) != null)
	ASSERT_TRUE(tile_x(0, 1, 0).get_way(wt_road) != null)

	// the road next to the townhall disappears
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(0, 1, 0)), null)

	// a big city needs a bigger townhall
	ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0), coord3d(0, 0, 0), "1500"), null)
	sleep()
	ASSERT_TRUE(tile_x(0, 0, 0).find_object(mo_building) != null)

	// clean up: remove everything the city has built
	for (local pass = 0; pass < 3; ++pass) {
		for (local x = 0; x < 16; ++x) {
			for (local y = 0; y < 16; ++y) {
				command_x(tool_remover).work(player_x(1), coord3d(x, y, 0))
			}
		}
	}
	for (local x = 0; x < 16; ++x) {
		for (local y = 0; y < 16; ++y) {
			ASSERT_TRUE(tile_x(x, y, 0).is_empty())
		}
	}
	RESET_ALL_PLAYER_FUNDS()
}
