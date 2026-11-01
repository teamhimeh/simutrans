/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef UTILS_SATURATED_MATH_H
#define UTILS_SATURATED_MATH_H

#include "../simtypes.h"
#include <limits>

// Nonnegative quantities: clamp before narrowing, never after a wrapping cast.
template<class T> inline T saturate(uint64 value)
{
	const uint64 limit = (uint64)std::numeric_limits<T>::max();
	return (T)(value > limit ? limit : value);
}

inline sint64 saturated_add(sint64 a, sint64 b)
{
	const sint64 maximum = std::numeric_limits<sint64>::max();
	const sint64 minimum = std::numeric_limits<sint64>::min();
	if (b > 0 && a > maximum - b) { return maximum; }
	if (b < 0 && a < minimum - b) { return minimum; }
	return a + b;
}

// Compute a*b/divisor exactly, without requiring a double-width integer type.
// Preserve the fractional remainder until the final rounding operation.
inline uint64 saturated_mul_div(uint64 a, uint64 b, uint64 divisor, uint64 limit, bool round_up = false)
{
	if (divisor == 0) {
		return limit;
	}
	const uint64 whole = a / divisor;
	if (b != 0 && whole > limit / b) {
		return limit;
	}
	const uint64 base = whole * b;
	const uint64 fraction = a % divisor;
	uint64 quotient = 0;
	uint64 remainder = 0;
	if (fraction == 0 || b <= std::numeric_limits<uint64>::max() / fraction) {
		quotient = fraction * b / divisor;
		remainder = fraction * b % divisor;
	}
	else {
		// Long multiplication with a quotient and a remainder at every bit.
		for (int bit = 63; bit >= 0; --bit) {
			if (quotient > (limit - base) / 2) {
				return limit;
			}
			quotient *= 2;
			if (remainder >= divisor - remainder) {
				remainder -= divisor - remainder;
				if (quotient == limit - base) { return limit; }
				++quotient;
			}
			else {
				remainder *= 2;
			}
			if ((b >> bit) & 1) {
				if (remainder >= divisor - fraction) {
					remainder -= divisor - fraction;
					if (quotient == limit - base) { return limit; }
					++quotient;
				}
				else {
					remainder += fraction;
				}
			}
		}
	}
	if (quotient > limit - base) {
		return limit;
	}
	const uint64 result = base + quotient;
	return round_up && remainder != 0 && result < limit ? result + 1 : result;
}

#endif
