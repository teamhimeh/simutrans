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
