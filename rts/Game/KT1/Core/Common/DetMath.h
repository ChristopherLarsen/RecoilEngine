/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: DetMath - the Lua's number maths in the Lua's order of evaluation, so a ported rule computes bit for
//   bit what the Lua did (LUA-FREE-PLAN §2.2 p5, check mode §3.2).
// The Lua number in this engine is a float (rts/lib/lua/include/luaconf.h: LUA_NUMBER float), so every helper works in
//   LuaNumber (float): the same maths in double rounds differently and check mode would show differences.
// Why: floating-point results depend on the order of the operations. Each helper is written as the Lua expression it
//   replaces (named in its comment), so a port reads `DetMath::Lerp(a, b, t)` for the Lua's `a + (b - a) * t`.
// Rules: only + - * / and std::sqrt / std::floor, which IEEE 754 makes exact on every client (the Lua's math::floor and
//   math::sqrt give the same results). No trig, exp or pow: the C library's versions differ between builds; synced code
//   takes those from the engine's own math. The engine builds x86-64 with SSE math (float operations round to float)
//   and no fused multiply-add; the Core tests build with -ffp-contract=off to match.
//   The engine also builds with -fsingle-precision-constant: a bare floating literal is a float. Core code builds its
//   constants from integers (HALF) so their type is visible.
// No engine include, no allocation.

#ifndef KT1_CORE_COMMON_DET_MATH_H
#define KT1_CORE_COMMON_DET_MATH_H

#include <cmath>
#include <span>

namespace KT1::Core::DetMath {

// the Lua's number type (luaconf.h LUA_NUMBER)
using LuaNumber = float;

inline constexpr LuaNumber HALF = static_cast<LuaNumber>(1) / 2;

// Lua: a + (b - a) * t
inline LuaNumber Lerp(LuaNumber a, LuaNumber b, LuaNumber t) { return a + (b - a) * t; }

// Lua: math.max(lo, math.min(hi, x))
inline LuaNumber Clamp(LuaNumber x, LuaNumber lo, LuaNumber hi) {
	const LuaNumber m = (x < hi) ? x : hi;   // math.min(hi, x) returns hi unless x < hi
	return (lo < m) ? m : lo;                // math.max(lo, m) returns lo unless lo < m
}

// Lua: math.floor(x)
inline LuaNumber Floor(LuaNumber x) { return std::floor(x); }

// Lua: math.floor(x + 0.5), the game's rounding (halves go up: -0.5 -> 0, 2.5 -> 3)
inline LuaNumber RoundHalfUp(LuaNumber x) { return std::floor(x + HALF); }

// Lua 5.1 (luai_nummod): a % b == a - math.floor(a / b) * b (takes the sign of b: -1 % 3 == 2)
inline LuaNumber Mod(LuaNumber a, LuaNumber b) { return a - std::floor(a / b) * b; }

// Lua: ax * bx + az * bz
inline LuaNumber Dot2(LuaNumber ax, LuaNumber az, LuaNumber bx, LuaNumber bz) { return ax * bx + az * bz; }

// Lua: ax * bx + ay * by + az * bz (left to right)
inline LuaNumber Dot3(LuaNumber ax, LuaNumber ay, LuaNumber az, LuaNumber bx, LuaNumber by, LuaNumber bz) {
	return ax * bx + ay * by + az * bz;
}

// Lua: math.sqrt(dx * dx + dz * dz)
inline LuaNumber Dist2(LuaNumber dx, LuaNumber dz) { return std::sqrt(dx * dx + dz * dz); }

// Lua: math.sqrt(dx * dx + dy * dy + dz * dz)
inline LuaNumber Dist3(LuaNumber dx, LuaNumber dy, LuaNumber dz) { return std::sqrt(dx * dx + dy * dy + dz * dz); }

// Lua: local s = 0; for i = 1, #v do s = s + v[i] end (front to back; callers pass values in ascending unit id)
inline LuaNumber Sum(std::span<const LuaNumber> v) {
	LuaNumber s = 0;
	for (const LuaNumber x: v)
		s = s + x;
	return s;
}

} // namespace KT1::Core::DetMath

#endif
