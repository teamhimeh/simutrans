//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Saving must fail cleanly (no crash, an error is returned, an existing save and no
// temporary file are left changed) when the temporary file cannot be written.
// Format: binary. run-automated-tests.sh selects the save format and prepares, per test file:
//   save/save_failure_disk_full.sv_   -> symlink to /dev/full (Linux only; skipped elsewhere)
//   save/save_failure_unwritable.sv_  -> a directory, so it cannot be opened for writing
// It also puts an existing save/<name>.sve next to each and afterwards checks that it is unchanged.
//


function test_save_failure_binary_disk_full()
{
	ASSERT_EQUAL(world.save_game("save_failure_disk_full"), "Saving failed")
}


function test_save_failure_binary_unwritable()
{
	ASSERT_EQUAL(world.save_game("save_failure_unwritable"), "Saving failed")
}
