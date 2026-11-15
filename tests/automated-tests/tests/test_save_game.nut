//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for world.save_game(): saving by name from a script
//

function test_save_game_valid_name()
{
	ASSERT_EQUAL(world.save_game("script_save_test"), null)
	// an existing save is replaced
	ASSERT_EQUAL(world.save_game("script_save_test"), null)
	// inner spaces, dots and dashes
	ASSERT_EQUAL(world.save_game("script save-test.v2"), null)
}


function test_save_game_multibyte_name()
{
	ASSERT_EQUAL(world.save_game("script_save_\u30c6\u30b9\u30c8"), null)
	ASSERT_EQUAL(world.save_game("script_save_\u00e9"), null)
}


function test_save_game_rejects_directory_traversal()
{
	local names = [
		"..", ".", "../script_save_escape", "..\\script_save_escape", "a/b", "a\\b",
		"/script_save_abs", "C:script_save_abs", "C:\\script_save_abs", "\\\\server\\share\\x",
		"a/../../script_save_escape", ".script_save_hidden"
	]
	foreach (name in names) {
		local result = world.save_game(name)
		ASSERT_TRUE(result != null)
		ASSERT_TRUE(result.len() > 0)
	}
}


function test_save_game_rejects_forbidden_characters()
{
	foreach (c in [":", "*", "?", "\"", "<", ">", "|"]) {
		ASSERT_TRUE(world.save_game("script_save_" + c + "x") != null)
	}
	ASSERT_TRUE(world.save_game("script_save ") != null)
	ASSERT_TRUE(world.save_game(" script_save") != null)
	ASSERT_TRUE(world.save_game("script_save.") != null)
}


function test_save_game_rejects_reserved_names()
{
	foreach (name in ["CON", "con", "PRN", "AUX", "NUL", "COM1", "lpt9", "con.txt", "NUL.sve"]) {
		ASSERT_TRUE(world.save_game(name) != null)
	}
}


function test_save_game_rejects_control_and_invisible_characters()
{
	foreach (c in ["\x01", "\n", "\r", "\t", "\x1b", "\x7f", "\u0085", "\u202e", "\u200b", "\u2028", "\ufeff"]) {
		ASSERT_TRUE(world.save_game("script_save_" + c + "x") != null)
	}
	// NUL must not cut the name short: "script_save_ok\0evil" would otherwise be saved as "script_save_ok"
	ASSERT_TRUE(world.save_game("script_save_ok" + format("%c", 0) + "evil") != null)
}


function test_save_game_rejects_empty_and_long_names()
{
	ASSERT_TRUE(world.save_game("") != null)
	local long_name = ""
	for (local i = 0; i < 129; i++) {
		long_name += "a"
	}
	ASSERT_TRUE(world.save_game(long_name) != null)
}


function test_save_game_rejects_non_string()
{
	local failed = false
	try {
		world.save_game(42)
	}
	catch (e) {
		failed = true
	}
	ASSERT_TRUE(failed)
}


// run-automated-tests.sh creates save/script_save_readonly.sve as a read-only file
// and afterwards checks that its content is unchanged and that no temporary file is left.
function test_save_game_rejects_read_only_target()
{
	local result = world.save_game("script_save_readonly")
	ASSERT_TRUE(result != null)
	ASSERT_TRUE(result.len() > 0)
}
