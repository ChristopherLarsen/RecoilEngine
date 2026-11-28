/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core: the hull turn to aim (LUA-FREE S19, COMBAT family), ported from LuaRules/Gadgets/cmb_hullturn.lua as the
// Lua has it (owner 2026-11-28: port, then correct). No engine include: tested engine-free (Test/HullTurn).
//
// REQ MOV-005 v3 R6: a unit with no move order whose target lies outside the arc of every weapon that reaches it turns in
// place, at no more than its turn rate, until an arc covers it. REQ MOV-005 v4 R7: a hover unit under a move order turns
// its hull towards the target while it follows its route (the move type's supcomHeadingFree), and hands the hull back
// once it faces the way it travels. Hold fire (ORD-002 v1 R4) never turns.
//
// Update() is one call of the Lua's Update(r): the adapter reads the unit into a View, Update decides, the adapter applies
// the Out (a heading and up to two heading-free writes, in order). Every number is the Lua number (float), in the Lua's
// order; sin, cos and atan2 come from the engine (Trig), as the Lua's math library takes them.

#ifndef KT1_CORE_HULL_TURN_CORE_H
#define KT1_CORE_HULL_TURN_CORE_H

#include <array>
#include <cstdint>

#include "Game/KT1/Core/Common/DetMath.h"

namespace KT1::Core::HullTurn {

using DetMath::LuaNumber;

// the engine's trig (math::atan2, math::sin, math::cos, math::PI): the Lua's math library calls the same
struct Trig {
	LuaNumber (*atan2)(LuaNumber y, LuaNumber x);
	LuaNumber (*sin)(LuaNumber a);
	LuaNumber (*cos)(LuaNumber a);
	LuaNumber pi;
};

constexpr int MAX_WEAPONS = 16;

struct Weapon {
	LuaNumber arc = 0;   // radians either side of the hull's front (customParams.yaw_range, degrees, default 180)
	LuaNumber tol = 0;   // radians (customParams.fire_tolerance, degrees, default 2)
};

// per unit def, made once at Start; only defs that can move, cannot fly, turn, and have a weapon with an arc under 180
struct DefSpec {
	LuaNumber step = 0;   // radians per step: turnRate * TAU / 65536 (the engine's turn rate is in angle units per step)
	bool hover = false;   // moveDef name holds "hover", or customParams.supcom_hover_height is set
	int32_t weapons = 0;
	std::array<Weapon, MAX_WEAPONS> w{};
};

// the rule's constants (config, gamedata/kt1/hullturn.lua; labels there)
struct Params {
	LuaNumber rest = 0;   // elmos per step: below this a unit counts as at rest
	LuaNumber done = 0;   // radians: a turn that brought the bearing this close to the arc edge has arrived
	LuaNumber axis = 0;   // radians kept off an exact axis direction
};

// Lua: (a + PI) % TAU - PI
inline LuaNumber Wrap(LuaNumber a, const Trig& t) {
	const LuaNumber tau = 2 * t.pi;
	return DetMath::Mod(a + t.pi, tau) - t.pi;
}

// the per-unit state the Lua keeps in its list entry
struct UnitState {
	bool turning = false;
	bool free = false;     // r.free: the gadget holds the hull direction (R7)
};

// what the order queue's first command is, as the rule sees it
enum class Cmd : uint8_t { None, Attack, Moving, Other };

struct View {
	Cmd cmd = Cmd::None;
	int32_t nparams = 0;          // the first command's parameter count
	LuaNumber p1 = 0, p3 = 0;     // its first and third parameter (p3 only when nparams >= 3)
	bool p1Unit = false;          // ATTACK with a unit: Spring.ValidUnitID(p1)
	LuaNumber p1x = 0, p1z = 0;   // that unit's position
	int32_t fireState = 0;
	LuaNumber vx = 0, vz = 0, speed = 0;   // velocity and its length (Spring.GetUnitVelocity)
	LuaNumber ux = 0, uz = 0;              // position
	LuaNumber dx = 0, dz = 0;              // front direction
	// the first weapon whose engine target is a unit (kind 1) or a ground point (kind 2), in weapon order
	int32_t wkind = 0;                     // 0: none
	LuaNumber wx = 0, wz = 0;              // its position
	int32_t weapons = 0;
	std::array<LuaNumber, MAX_WEAPONS> range{};   // Spring.GetUnitWeaponState(u, n, "range")
	bool groundMoveType = true;            // the move type is a ground one (else SetGroundMoveTypeData raises)
};

// what Update asks the engine to do, in order: the heading-free writes, then the heading (the two touch different state)
struct Out {
	int32_t freeWrites = 0;
	std::array<bool, 2> free{};
	bool face = false;
	LuaNumber fx = 0, fz = 0;   // Spring.SetUnitDirection(u, fx, 0, fz)
	LuaNumber faceA = 0;        // the heading (radians from +z) it was made from: the check column
};

// the gadget-wide state: false once the engine refused the heading-free switch (the Lua's switchOK)
struct Shared {
	bool switchOK = true;
};

namespace Detail {
	// Lua SetFree(r, on)
	inline void SetFree(UnitState& r, bool on, const View& v, Shared& sh, Out& o) {
		if (!sh.switchOK) return;
		if (!v.groundMoveType) { sh.switchOK = false; return; }
		o.free[o.freeWrites++] = on;
		r.free = on;
	}
	// Lua Face(u, a, sign)
	inline void Face(LuaNumber a, int32_t sign, const Params& p, const Trig& t, Out& o) {
		const LuaNumber quarter = t.pi / 2;
		const LuaNumber q = a / quarter;
		const LuaNumber nearest = DetMath::Floor(q + DetMath::HALF);
		const LuaNumber off = q - nearest;
		if ((off < 0 ? -off : off) * quarter < p.axis) a = nearest * quarter - static_cast<LuaNumber>(sign) * p.axis;
		o.face = true;
		o.faceA = a;
		o.fx = t.sin(a);
		o.fz = t.cos(a);
	}
	inline LuaNumber Abs(LuaNumber x) { return x < 0 ? -x : x; }

	// Lua Return(r, gx, gz)
	inline void Return(UnitState& r, const DefSpec& s, bool hasGoal, LuaNumber gx, LuaNumber gz, const View& v,
		const Params& p, const Trig& t, Shared& sh, Out& o) {
		bool have = false;
		LuaNumber want = 0;
		if (v.speed > p.rest) {
			want = t.atan2(v.vx, v.vz);
			have = true;
		} else if (hasGoal) {
			const LuaNumber ex = gx - v.ux, ez = gz - v.uz;
			if (ex * ex + ez * ez > 1) { want = t.atan2(ex, ez); have = true; }
		}
		if (!have) { SetFree(r, false, v, sh, o); return; }
		const LuaNumber head = t.atan2(v.dx, v.dz);
		const LuaNumber off = Wrap(want - head, t);
		const int32_t sign = off > 0 ? 1 : -1;
		if (Abs(off) <= s.step) {
			Face(want, sign, p, t, o);
			SetFree(r, false, v, sh, o);
			return;
		}
		Face(head + static_cast<LuaNumber>(sign) * s.step, sign, p, t, o);
		r.turning = true;
	}
}

// Lua Update(r). The Lua passes p1, p3 of the first command to Return: a goal only when the command has them.
inline Out Update(UnitState& r, const DefSpec& s, const View& v, const Params& p, const Trig& t, Shared& sh) {
	using namespace Detail;
	Out o;
	const bool was = r.turning;
	r.turning = false;
	const bool travelling = s.hover && v.cmd == Cmd::Moving;
	const bool hasGoal = v.nparams >= 3;
	if (v.cmd != Cmd::None && v.cmd != Cmd::Attack && !travelling) {
		if (r.free) SetFree(r, false, v, sh, o);
		return o;
	}
	if (r.free && !travelling) SetFree(r, false, v, sh, o);
	if (v.fireState == 0) {
		if (r.free) { if (travelling) Return(r, s, hasGoal, v.p1, v.p3, v, p, t, sh, o); else SetFree(r, false, v, sh, o); }
		return o;
	}
	if (!travelling && v.speed > p.rest) return o;

	// the target: the attack order's (ground point or unit), else the first weapon's own target
	bool hasT = false;
	LuaNumber tx = 0, tz = 0;
	if (v.cmd == Cmd::Attack) {
		if (v.nparams >= 3) { hasT = true; tx = v.p1; tz = v.p3; }
		else if (v.p1Unit) { hasT = true; tx = v.p1x; tz = v.p1z; }
	} else if (v.wkind != 0) {
		hasT = true; tx = v.wx; tz = v.wz;
	}
	if (!hasT) {
		if (r.free) { if (travelling) Return(r, s, hasGoal, v.p1, v.p3, v, p, t, sh, o); else SetFree(r, false, v, sh, o); }
		return o;
	}
	const LuaNumber ex = tx - v.ux, ez = tz - v.uz;
	const LuaNumber have = t.atan2(v.dx, v.dz);
	const LuaNumber b = Wrap(t.atan2(ex, ez) - have, t);
	const LuaNumber d2 = ex * ex + ez * ez;
	bool hasNeed = false, covered = false;
	LuaNumber need = 0;
	for (int32_t n = 0; n < s.weapons; ++n) {
		const LuaNumber range = (n < v.weapons) ? v.range[n] : 0;
		if (range * range >= d2) {
			const LuaNumber over = Abs(b) - s.w[n].arc;
			if (over <= (was ? p.done : s.w[n].tol)) { covered = true; break; }
			if (!hasNeed || over < need) { need = over; hasNeed = true; }
		}
	}
	if (travelling) {
		if (!(covered || hasNeed)) {
			if (r.free) Return(r, s, hasGoal, v.p1, v.p3, v, p, t, sh, o);
			return o;
		}
		if (!r.free) SetFree(r, true, v, sh, o);
		if (!r.free) return o;
		r.turning = true;
		if (covered) return o;
	} else if (covered || !hasNeed) {
		return o;
	}
	const int32_t sign = b > 0 ? 1 : -1;
	const LuaNumber turn = (s.step < need) ? s.step : need;   // Lua math.min(s.step, need)
	Face(have + static_cast<LuaNumber>(sign) * turn, sign, p, t, o);
	r.turning = true;
	return o;
}

// Lua: r.turning or (f + r.u) % CHECK == 0
inline bool Due(const UnitState& r, int32_t frame, int32_t id, int32_t checkSteps) {
	return r.turning || (frame + id) % checkSteps == 0;
}

} // namespace KT1::Core::HullTurn

#endif
