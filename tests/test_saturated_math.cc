/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 *
 * Standalone arithmetic regression test (GCC/Clang):
 * g++ -std=c++11 -O2 tests/test_saturated_math.cc -o build/test_saturated_math
 */

#include "../utils/saturated_math.h"
#include <cassert>
#include <cstdio>
#include <random>

static void check(uint64 a, uint64 b, uint64 d, uint64 limit)
{
	for (int round_up = 0; round_up != 2; ++round_up) {
		const unsigned __int128 numerator = (unsigned __int128)a * b;
		unsigned __int128 expected = numerator / d;
		if (round_up && numerator % d) { ++expected; }
		if (expected > limit) { expected = limit; }
		assert(saturated_mul_div(a, b, d, limit, round_up) == expected);
	}
}

int main()
{
	const uint64 i32 = std::numeric_limits<sint32>::max();
	const uint64 u32 = std::numeric_limits<uint32>::max();
	const uint64 i64 = std::numeric_limits<sint64>::max();
	const uint64 u64 = std::numeric_limits<uint64>::max();
	assert(saturated_add(i64, 1) == (sint64)i64);
	assert(saturated_add(std::numeric_limits<sint64>::min(), -1) == std::numeric_limits<sint64>::min());
	assert(saturated_add(-100, 99) == -1);
	assert(saturate<sint32>(i32 - 1) == i32 - 1);
	assert(saturate<sint32>(i32) == i32);
	assert(saturate<sint32>(i32 + 1) == i32);
	assert(saturate<sint32>(i32 + 131069) == i32);
	assert(saturate<sint32>((i32 - 1) + 1) == i32);
	assert(saturate<sint32>((uint64)saturate<sint32>(i32 + 1) + 65535) == i32);
	assert(saturate<uint32>(i32 * 4096) == u32);
	assert(saturate<uint32>(u32 + 1) == u32);
	assert(saturate<uint32>((uint64)2097152 * 1024) == (uint64)2147483648);
	check(u32 * 196861, 1 << 16, 1 << 24, u32); // Boosted power and work.
	check(i32 * 196861, u32, 1 << 16, i32); // Maximum production and delta_t.

	const uint64 values[] = {0, 1, 2, 255, 256, 65535, 131072, i32 - 1, i32, i32 + 1, u32, i64, u64};
	for (uint64 a : values) {
		for (uint64 b : values) {
			for (uint64 d : values) {
				if (d) { check(a, b, d, i32); check(a, b, d, i64); check(a, b, d, u64); }
			}
		}
	}
	std::mt19937_64 random(768);
	for (int n = 0; n < 100000; ++n) {
		const uint64 a = random(), b = random(), d = random() | 1, limit = random();
		check(a, b, d, limit);
	}
	// Production carries fractional effort into the next step even at saturation.
	for (int n = 0; n < 10000; ++n) {
		const uint64 base_boost = (random() & i32) * (256 + random() % 196606);
		const uint64 delta = random() & u32, mask = 65535, old_remainder = random() & mask;
		const uint64 remainder = ((base_boost & mask) * (delta & mask) & mask) + old_remainder;
		const uint64 actual = saturate<sint32>(saturated_mul_div(base_boost, delta, mask + 1, i32) + (remainder >> 16));
		const unsigned __int128 exact = (unsigned __int128)base_boost * delta + old_remainder;
		assert(actual == (exact >> 16 > i32 ? i32 : (uint64)(exact >> 16)));
		assert((remainder & mask) == (exact & mask));
	}

	for (int bits : {18, 20, 31}) {
		const uint64 monthly = i32 << (bits - 18);
		assert(monthly >> (bits - 18) == i32);
		check(monthly, 65535, 256, i64); // Script production API.
		check(i32 * 65535, (uint64)1 << bits, (uint64)1 << 18, 932067);
	}

	// Preserve the original storage rounding, including productivity != 1.
	for (uint64 productivity : {1, 10, 65535}) {
		for (uint64 factor : {1, 256, 65535}) {
			for (uint64 base : values) {
				if (base > i32) { continue; }
				const unsigned __int128 numerator = (unsigned __int128)65535 * base * (1 << 18);
				unsigned __int128 expected = (numerator + factor - 1) / (productivity * factor);
				if (expected > i32) { expected = i32; }
				const uint64 actual = saturated_mul_div(65535 * base, 1 << 18, factor, i32 * productivity, true) / productivity;
				assert(actual == expected);
			}
		}
	}
	std::puts("Saturated arithmetic: boundary and 100000 randomized cases passed");
}
