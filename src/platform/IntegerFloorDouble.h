#pragma once

#include "java/Type.h"

#include <cstring>

// floor(double) -> int, computed from the IEEE-754 fields with integer
// arithmetic, for cores with no double-precision FPU.
//
// This returns exactly what MathHelper::floor_double's original expression
// returns for every input:
//
//     int_t i = JavaArithmetic::doubleToInt(d);
//     return d < (double)i ? JavaArithmetic::intSub(i, 1) : i;
//
// including NaN, both infinities, denormals, both zeros and the saturation
// boundaries. MathHelperFloorTests.cpp holds that equivalence against the
// original expression over eight million values.
//
// Kept in its own header rather than inline in MathHelper.cpp so the test can
// exercise this code rather than a copy of it. See PS2_INTEGER_FLOOR_DOUBLE for
// the cost that motivates it.
//
// Layout: bit 63 sign, bits 62-52 biased exponent, bits 51-0 mantissa.
inline int_t platformIntegerFloorDouble(double d)
{
	ulong_t bits;
	std::memcpy(&bits, &d, sizeof(bits));
	const int_t exponent = (int_t)((bits >> 52) & 0x7FFu) - 1023;
	const bool negative = (bits >> 63) != 0;

	// |d| < 1. Shifting the sign out distinguishes the two zeros from every
	// other small value, so -0.0 floors to 0 while -0.25 floors to -1. Denormals
	// have a zero exponent field, which biases to -1023 and lands here too.
	if (exponent < 0)
		return (negative && (bits << 1) != 0) ? -1 : 0;

	// Out of int range, plus the infinities and NaN.
	if (exponent >= 31)
	{
		// Java's (int) cast maps NaN to 0, and floor's comparison is false for
		// it, so the result is 0 rather than either clamp.
		if (exponent == 1024 && (bits & 0xFFFFFFFFFFFFFull) != 0)
			return 0;
		if (!negative)
			return 2147483647;
		// -2^31 is exactly representable and is its own floor.
		if (bits == 0xC1E0000000000000ull)
			return -2147483647 - 1;
		// Below that, (int)d saturates to INT_MIN and floor subtracts one from
		// it, which wraps to INT_MAX. That wrap is Java's, and it is what the
		// original expression produces through JavaArithmetic::intSub.
		return 2147483647;
	}

	// 1 <= |d| < 2^31: shift the implicit-one mantissa down to an integer and
	// round toward negative infinity by hand.
	const ulong_t mantissa = (bits & 0xFFFFFFFFFFFFFull) | 0x10000000000000ull;
	const int_t shift = 52 - exponent;
	const int_t truncated = (int_t)(mantissa >> shift);
	if (!negative)
		return truncated;
	const bool hasFraction = (mantissa & ((1ull << shift) - 1ull)) != 0;
	return hasFraction ? -truncated - 1 : -truncated;
}
