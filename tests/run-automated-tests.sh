#!/bin/bash
set -e

# Automated test runner for Simutrans OTRP (macOS/Linux compatible)
# This script runs automated tests and monitors for completion or failure
#
# Usage:
#   WORKDIR=/path/to/simutrans-test-base SIM_BINARY=./sim ./run-automated-tests.sh [test_func_name]
#
# Arguments:
#   test_func_name  (optional) Run only the specified test function.
#                   Example: test_label, test_building_build_house_random

# Check required environment variables
if [ -z "$WORKDIR" ]; then
	echo "Error: WORKDIR environment variable must be set"
	echo "Usage: WORKDIR=/path/to/simutrans-test-base SIM_BINARY=./sim ./run-automated-tests.sh [test_func_name]"
	exit 1
fi

SIM_BINARY="${SIM_BINARY:-../sim}"
TEST_FUNC_NAME="${1:-}"

# Determine script directory and paths
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
TEST_SRC="$SCRIPT_DIR/automated-tests"
SCENARIO_DIR="$WORKDIR/pak/scenario/automated-tests"

# Copy test files to WORKDIR (always use latest)
mkdir -p "$SCENARIO_DIR"
cp -r "$TEST_SRC"/* "$SCENARIO_DIR/"

# Terminate a running sim instance.
#
# On Windows/MSYS2 the plain `kill` sends an MSYS signal, but sim.exe is a native Win32 GUI
# binary: the signal takes about 10 seconds to land, and while a modal dialog owns the message
# loop it never lands at all -- the window then has to be closed by hand before the run
# continues. taskkill on the Win32 pid (the WINPID column of `ps`) terminates it immediately.
# Falls back to `kill` on macOS/Linux, where taskkill does not exist.
kill_sim() {
	local pid=$1

	if command -v taskkill > /dev/null 2>&1; then
		local winpid
		winpid=$(ps -p "$pid" 2>/dev/null | awk 'NR==2 {print $4}')
		# only ever target this one pid, never `taskkill //IM sim.exe`:
		# the developer may well have their own Simutrans open next to the test run
		if [ -n "$winpid" ] && taskkill //PID "$winpid" //F > /dev/null 2>&1; then
			wait "$pid" 2>/dev/null || true
			return
		fi
	fi

	kill "$pid" 2>/dev/null || true
	wait "$pid" 2>/dev/null || true
}

# Helper function to run a single test
run_single_test() {
	local test_name=$1
	local test_file_base=$2

	# Generate filtered all_tests.nut for this single test
	cat > "$SCENARIO_DIR/all_tests.nut" <<EOF
include("tests/${test_file_base}")

all_tests <- [
	${test_name}
]

failing_tests <- []
EOF

	# Use temporary file for monitoring
	local temp_log=$(mktemp)

	# sim block-buffers its stdout once it is redirected to a file and only flushes on a clean
	# exit, so the completion banner never shows up there while the game is still running. On
	# Windows that meant the run sat here for the full timeout unless the window was closed by
	# hand -- closing it was what flushed the buffer and let the test be recognised as passed.
	# The scenario VM writes its own log as it goes (dataobj/scenario.cc: "script-scenario.log"),
	# so watch that for progress and keep the stdout capture only for the failure dump.
	# Delete the previous run's copy first, or the first grep would match the test before this one.
	local scenario_log="$WORKDIR/script-scenario.log"
	rm -f "$scenario_log"

	# test_save_game needs a read-only savegame to be refused (see test_save_game_rejects_read_only_target)
	local ro_file="$WORKDIR/save/script_save_readonly.sve"
	if [ "$test_file_base" = "test_save_game" ]; then
		mkdir -p "$WORKDIR/save"
		rm -f "$ro_file" "$WORKDIR/save/script_save_readonly.sv_"
		printf 'read-only fixture' > "$ro_file"
		chmod 444 "$ro_file"
	fi

	# test_save_failure_<format> checks that a save which cannot be written fails cleanly.
	# Per test file: select the save format, then put in place
	#   save/save_failure_disk_full.sv_  -> symlink to /dev/full (the temporary file of a full disk)
	#   save/save_failure_unwritable.sv_ -> a directory (the temporary file cannot be opened)
	# and, next to each, an existing save that must survive the failed save unchanged.
	# /dev/full exists on Linux only: elsewhere the test is skipped (reported as passed).
	local sf_dir="$WORKDIR/save"
	local sf_config="$WORKDIR/config/simuconf.tab"
	local sf_format=""
	case "$test_file_base" in
		test_save_failure_bzip) sf_format="bzip2" ;; # no digit in the test names: CI picks them with test_[a-z_]+
		test_save_failure_*) sf_format="${test_file_base#test_save_failure_}" ;;
	esac
	if [ -n "$sf_format" ]; then
		if [ "$(uname -s)" != "Linux" ] || [ ! -c /dev/full ]; then
			echo "  - skipped (needs /dev/full, Linux only)"
			rm -f "$temp_log"
			return 0
		fi
		mkdir -p "$sf_dir"
		# a backup left by an interrupted run holds the original config: restore it first
		[ -f "$sf_config.save_failure_bak" ] && mv -f "$sf_config.save_failure_bak" "$sf_config"
		cp "$sf_config" "$sf_config.save_failure_bak"
		sed -i "s/^saveformat *=.*/saveformat = $sf_format/" "$sf_config"
		if ! grep -q "^saveformat = ${sf_format}[[:space:]]*$" "$sf_config"; then
			echo "  cannot select save format $sf_format in $sf_config"
			mv -f "$sf_config.save_failure_bak" "$sf_config"
			return 1
		fi
		local sf_name
		for sf_name in save_failure_disk_full save_failure_unwritable; do
			rm -rf "$sf_dir/$sf_name.sv_"
			printf 'existing save' > "$sf_dir/$sf_name.sve"
		done
		ln -s /dev/full "$sf_dir/save_failure_disk_full.sv_"
		mkdir "$sf_dir/save_failure_unwritable.sv_"
	fi

	# Start sim in background and redirect output
	$SIM_BINARY -set_workdir "$WORKDIR" -objects pak -scenario automated-tests -debug 2 -lang en -fps 100 > "$temp_log" 2>&1 &
	local pid=$!

	local result=1
	local timeout=300  # 5 minutes timeout
	local elapsed=0

	while [ $elapsed -lt $timeout ]; do
		sleep 1
		elapsed=$((elapsed + 1))

		# Check if process is still running (macOS compatible)
		if ! ps -p $pid > /dev/null 2>&1; then
			# exiting flushes the stdout buffer, so by now either log may carry the banner
			if grep -q 'Tests completed successfully.' "$scenario_log" "$temp_log" 2>/dev/null; then
				result=0
			else
				result=1
			fi
			break
		fi

		# Check for successful completion
		if grep -q 'Tests completed successfully.' "$scenario_log" 2>/dev/null; then
			kill_sim $pid
			result=0
			break
		fi

		# Check for test failures (assertions failed but test runner completed)
		if grep -q 'Failed tests:' "$scenario_log" 2>/dev/null; then
			kill_sim $pid
			result=1
			break
		fi

		# Check for errors. These are dbg->warning() output and therefore land on the buffered
		# stdout, so this rarely fires before the process exits -- the checks above and the
		# timeout are what actually end a broken run.
		if grep -q 'error \[Call function failed\] calling' "$temp_log" 2>/dev/null || \
		   grep -q 'error \[Reading / compiling script failed\] calling' "$temp_log" 2>/dev/null || \
		   grep -q '</error>' "$temp_log" 2>/dev/null; then
			kill_sim $pid
			result=1
			break
		fi
	done

	# Timeout check
	if [ $elapsed -ge $timeout ]; then
		kill_sim $pid
		result=1
	fi

	# A refused save must leave the read-only file untouched and no temporary file behind
	if [ "$test_file_base" = "test_save_game" ]; then
		if [ $result -eq 0 ] && { [ "$(cat "$ro_file")" != "read-only fixture" ] || [ -e "$WORKDIR/save/script_save_readonly.sv_" ]; }; then
			echo "  read-only savegame was modified or a temporary file was left behind"
			result=1
		fi
		chmod 644 "$ro_file" 2>/dev/null
		rm -f "$ro_file" "$WORKDIR/save/script_save_readonly.sv_"
	fi

	# A failed save must leave the existing saves untouched and no temporary file behind
	if [ -n "$sf_format" ]; then
		for sf_name in save_failure_disk_full save_failure_unwritable; do
			if [ $result -eq 0 ] && [ "$(cat "$sf_dir/$sf_name.sve")" != "existing save" ]; then
				echo "  $sf_name.sve was modified by the failed save"
				result=1
			fi
		done
		# in the disk full case the symlink to /dev/full is the temporary file itself and has to be removed
		if [ $result -eq 0 ] && [ "${test_name%_disk_full}" != "$test_name" ] && { [ -e "$sf_dir/save_failure_disk_full.sv_" ] || [ -L "$sf_dir/save_failure_disk_full.sv_" ]; }; then
			echo "  temporary file save_failure_disk_full.sv_ was left behind"
			result=1
		fi
		mv -f "$sf_config.save_failure_bak" "$sf_config"
		rm -rf "$sf_dir/save_failure_disk_full.sve" "$sf_dir/save_failure_disk_full.sv_" \
		       "$sf_dir/save_failure_unwritable.sve" "$sf_dir/save_failure_unwritable.sv_"
	fi

	# If test failed, show the last 30 lines
	if [ $result -ne 0 ]; then
		echo "  ✗ FAILED"
		echo "  Last 30 lines of scenario log:"
		echo "  ----------------------------------------"
		tail -30 "$scenario_log" 2>/dev/null | sed 's/^/  /'
		echo "  ----------------------------------------"
		echo "  Last 30 lines of sim output:"
		echo "  ----------------------------------------"
		tail -30 "$temp_log" 2>/dev/null | sed 's/^/  /'
		echo "  ----------------------------------------"
	else
		echo "  ✓ PASSED"
	fi

	rm -f "$temp_log"
	return $result
}

# If a specific test function is requested, run only that test
if [ -n "$TEST_FUNC_NAME" ]; then
	# Find which test file defines this function
	TEST_FILE=$(grep -rl "^function ${TEST_FUNC_NAME}()" "$TEST_SRC/tests/" 2>/dev/null | head -1)

	if [ -z "$TEST_FILE" ]; then
		echo "Error: Test function '${TEST_FUNC_NAME}' not found in any test file under $TEST_SRC/tests/"
		exit 1
	fi

	# Get the basename without extension (e.g., test_building)
	TEST_FILE_BASE=$(basename "$TEST_FILE" .nut)

	echo "Running single test: $TEST_FUNC_NAME (from $TEST_FILE_BASE.nut)"
	echo "Working directory: $WORKDIR"
	echo "Sim binary: $SIM_BINARY"
	echo ""

	run_single_test "$TEST_FUNC_NAME" "$TEST_FILE_BASE"
	exit $?
fi

# No specific test requested - run all tests, each in its own game instance
echo "Running all tests (each in its own game instance)..."
echo "Working directory: $WORKDIR"
echo "Sim binary: $SIM_BINARY"
echo ""

# Extract test function names from all_tests array only (not failing_tests)
# This extracts lines between "all_tests <- [" and the first "]"
ALL_TEST_FUNCS=$(awk '/^all_tests <- \[/,/^\]/' "$TEST_SRC/all_tests.nut" | grep -E '^\s+test_' | sed 's/^[[:space:]]*//' | sed 's/,$//')

if [ -z "$ALL_TEST_FUNCS" ]; then
	echo "Error: No tests found in all_tests array"
	exit 1
fi

# Count total tests
TOTAL_TESTS=$(echo "$ALL_TEST_FUNCS" | wc -l | tr -d ' ')
CURRENT_TEST=0
FAILED_TESTS=()

# Run each test in its own game instance
for test_func in $ALL_TEST_FUNCS; do
	CURRENT_TEST=$((CURRENT_TEST + 1))

	# Find which test file defines this function
	TEST_FILE=$(grep -rl "^function ${test_func}()" "$TEST_SRC/tests/" 2>/dev/null | head -1)

	if [ -z "$TEST_FILE" ]; then
		echo "[$CURRENT_TEST/$TOTAL_TESTS] $test_func"
		echo "  ✗ FAILED (test file not found)"
		FAILED_TESTS+=("$test_func")
		continue
	fi

	TEST_FILE_BASE=$(basename "$TEST_FILE" .nut)

	echo "[$CURRENT_TEST/$TOTAL_TESTS] $test_func"

	if ! run_single_test "$test_func" "$TEST_FILE_BASE"; then
		FAILED_TESTS+=("$test_func")
	fi
done

echo ""
echo "========================================"
echo "Tests completed: $TOTAL_TESTS total, ${#FAILED_TESTS[@]} failed"

if [ ${#FAILED_TESTS[@]} -eq 0 ]; then
	echo "Result: SUCCESS"
	echo "========================================"
	exit 0
else
	echo "Result: FAILURE"
	echo ""
	echo "Failed tests:"
	for test_name in "${FAILED_TESTS[@]}"; do
		echo "  - $test_name"
	done
	echo "========================================"
	exit 1
fi
