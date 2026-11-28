/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: DetMath - double helpers that pin the order of evaluation, and the Lua 5.1 forms of floor, mod and
//   rounding, so a ported rule computes bit for bit what the Lua did (LUA-FREE-PLAN §2.2 p5, check mode §3.2).
// Why: floating-point results depend on the order of the operations. Each helper is written as the Lua expression it
//   replaces (named in its comment), so a port reads `DetMath::Lerp(a, b, t)` for the Lua's `a + (b - a) * t`.
// Rules: only + - * / and std::sqrt / std::floor, which IEEE 754 makes exact on every client. No trig, exp or pow:
//   the C library's versions differ between builds; synced code takes those from the engine's own math. The engine
//   builds x86-64 with SSE math and no fused multiply-add; the Core tests build with -ffp-contract=off to match.
// No engine include, no allocation.

#ifndef KT1_CORE_COMMON_DET_MATH_H
#define KT1_CORE_COMMON_DET_MATH_H

#include <cmath>
#include <span>

namespace KT1::Core::DetMath {

// Lua: a + (b - a) * t
inline double Lerp(double a, double b, double t) { return a + (b - a) * t; }

// Lua: math.max(lo, math.min(hi, x))
inline double Clamp(double x, double lo, double hi) {
	const double m = (x < hi) ? x : hi;   // math.min(hi, x) returns hi unless x < hi
	return (lo < m) ? m : lo;             // math.max(lo, m) returns lo unless lo < m
}

// Lua: math.floor(x)
inline double Floor(double x) { return std::floor(x); }

// Lua: math.floor(x + 0.5), the game's rounding (halves go up: -0.5 -> 0, 2.5 -> 3)
inline double RoundHalfUp(double x) { return std::floor(x + 0.5); }

// Lua 5.1: a % b == a - math.floor(a / b) * b (takes the sign of b: -1 % 3 == 2)
inline double Mod(double a, double b) { return a - std::floor(a / b) * b; }

// Lua: ax * bx + az * bz
inline double Dot2(double ax, double az, double bx, double bz) { return ax * bx + az * bz; }

// Lua: ax * bx + ay * by + az * bz (left to right)
inline double Dot3(double ax, double ay, double az, double bx, double by, double bz) {
	return ax * bx + ay * by + az * bz;
}

// Lua: math.sqrt(dx * dx + dz * dz)
inline double Dist2(double dx, double dz) { return std::sqrt(dx * dx + dz * dz); }

// Lua: math.sqrt(dx * dx + dy * dy + dz * dz)
inline double Dist3(double dx, double dy, double dz) { return std::sqrt(dx * dx + dy * dy + dz * dz); }

// Lua: local s = 0; for i = 1, #v do s = s + v[i] end (front to back; callers pass values in ascending unit id)
inline double Sum(std::span<const double> v) {
	double s = 0.0;
	for (const double x: v)
		s = s + x;
	return s;
}

} // namespace KT1::Core::DetMath

#endif
