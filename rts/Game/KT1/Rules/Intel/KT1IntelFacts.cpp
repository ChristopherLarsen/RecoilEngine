/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: IntelFacts (version 1; LUA-FREE S06). int_coverage's facts pass with the facts held here, read on demand.
//
// Replaces:   the bulk copy of rts/Lua/SupComIntFacts.cpp (Spring.SupComIntFactsRun writes 27 columns into int_coverage's F
//             tables and two fields per source, every frame); that file stands down while this rule is on. The pass itself is
//             the same, line for line, with its pure part in Core/Intel/IntelFactsCore (cells, keys, stamp, shot record, reach).
// Switch:     mod option kt1_intelfacts: 0 (default), 1, check (gamedata/kt1/options/intelfacts.lua). Check is compute-only
//             (checkSafe, KT1Register.h): Lua runs its own pass and reads its own columns; this rule computes into its own,
//             places no circle and writes nothing; int_coverage compares the two (CompareFacts).
// Phase:      none of its own: int_coverage calls Run from its GameFrame where it called SupComIntFactsRun, and every reader
//             comes after it in that GameFrame (design §8). GameFrame here does nothing.
// Config:     the rule's numbers come from int_coverage through Init (int_values.lua, int_cell_values.lua, int_lag_values.lua);
//             times in seconds, converted once to steps (IntelFactsCore Steps::From).
// Timer:      KT1::IntelFacts (each Run call).
// Lua (synced only), Spring.KT1.IntelFacts (each call in full: game-build docs/engine/rules/intel-facts.md):
//   Init(cfg) -> ok, Source(u, vr, wr, nv, sensors), UnitAnswers(u, upkeepOff, scale), Reset(), Slot(j, u), Fresh(u, on),
//   Unsettle(u), Run(cand, cn, part, f, stamp, all0, shadow, fin, drop, ev, F, lean) -> nRead, nStill, nShot, nFin, nDrop, nEv,
//   Fact(u) -> def, owner, sub, landed, px, pz, fcx, fcz, ccx, ccz; Fact(u, "dr", kind) -> dr, stamps (vision: and stf); Fact(u, "built"),
//   Circle(u), SensorCircle(u, kind) (check mode and cases).
// Determinism: units in the order Lua gives them (ascending slot), per-def and per-unit vectors, no hash order; float maths in
//             the Lua's order (Core), no FMA contraction.

#pragma GCC optimize("fp-contract=off")

#include <climits>
#include <string>
#include <vector>

#include "Game/KT1/Core/Intel/IntelFactsCore.h"
#include "Game/KT1/Rules/Intel/KT1IntelFactsLua.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/SupComIntIndex.h"
#include "Map/Ground.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {
namespace {

namespace C = IntelFactsCore;
using namespace IntelFactsLua;

constexpr int NONE = INT_MIN;
constexpr int NSENS = 4;                                                   // DESIGN: the sensor kinds, fixed order (size)
const char* const SENS_NAME[NSENS] = { "radar", "sonar", "omni", "water_vision" };
constexpr bool SENS_SWITCHED[NSENS] = { true, true, true, false };          // DESIGN: int_coverage SWITCHED (on/off-able)
constexpr bool SENS_POWER[NSENS] = { true, true, false, false };            // DESIGN: int_coverage POWER_FIELD (ECO-003 v7 R5)
constexpr int RADAR = 0, WATER_VISION = 3;                                  // DESIGN: indices of SENS_NAME
constexpr float EV_LANDED = 1.0f, EV_UNDER = 2.0f;                          // DESIGN: event codes of ev (int_landed, under water)

struct Placed {                       // a circle placed in a kind's index (VI.PutK's "same" test)
	int a = NONE; int mark = 0;
	float x = 0, z = 0, r2 = -1, ce = 0;
	bool placed = false;
};

struct Sens {                         // one sensor kind's source (int_coverage sources[u][kind])
	bool has = false; float r = 0;
	bool hasDr = false; float dr = 0;
	C::Stamped st; C::Shots sh;
	Placed pc;
	bool trail = false; float tlx = 0, tlz = 0, tr2 = 0, tce = 0;   // radar's leaving point (DESIGN)
};

struct Src {
	bool present = false;
	bool hasV = false, hasW = false, nv = false, anySens = false;
	float vr = 0, wr = 0;
	bool hasDr = false; float dr = 0;
	C::Stamped st; C::Shots sh;
	bool hasRk = false; float rkx = 0, rkz = 0, rdr = 0; int rda = NONE;
	int landed = -1, under = -1;      // -1: not yet sent
	int slotA = NONE; int mark = 0;   // the ally team its vision circle is placed for, its stamp
	bool trail = false; float tlx = 0, tlz = 0;
	bool calm = false;
	bool upOff = false; float scale = 1.0f;
	Sens sens[NSENS];
};

struct Unit {                         // per unit id
	bool fresh = false, built = false, nv = false;
	int slot = -1;
};

struct SlotRec {                      // per slot (int_coverage's F.slots index)
	int u = -1;                       // the unit Lua put there (Slot), -1: empty
	int readU = -1;                   // the unit last read in this slot (F.ids)
	bool hasRow = false;
	C::Row row;
};

// DESIGN: cfg.sensors, int_coverage's unitpass_intel_sensors: Lua does the sensor kinds, this pass does them, or shadow
constexpr int SENS_LUA = 0, SENS_ON = 1, SENS_SHADOW = 2;   // DESIGN

// The columns Run still writes into int_coverage's F (lean, SIM-CUTS-RANKED §9): the wide five for every unit read, the rest only
// for the units Lua finishes and the full-record defs (cloakers, jammers, fake contacts), or for every unit when lean is off.
enum Col { C_PX, C_PZ, C_PY, C_PDEF, C_POWN, C_PCLOAK, C_FCX, C_FCZ, C_CCX, C_CCZ, C_VX, C_VZ, C_VKEY, C_SKEY, C_FK, C_CK, C_FB,
	C_CB, C_PSUB, C_PLAND, C_N };
const char* const COL_NAME[C_N] = { "px", "pz", "py", "pdef", "pown", "pcloak", "fcx", "fcz", "ccx", "ccz", "vx", "vz", "vkey", "skey",
	"fk", "ck", "fb", "cb", "psub", "pland" };

struct Cfg {
	C::Rules rules;
	int sensors = SENS_LUA;
	bool waterSeesSub = false, unfinished = false, ok = false;
	float sensCell[NSENS] = {0, 0, 0, 0};
	std::vector<uint8_t> air, lua, cloak, onoff, full;   // per def
};

class IntelFacts : public Rule {
public:
	IntelFacts() : Rule(Info()) {}

	static RuleInfo Info() {
		RuleInfo i{"IntelFacts", 1, true, "LuaRules/Gadgets/int_coverage.lua (facts copy of SupComIntFactsRun)", false};
		i.checkSafe = true;                                                // compute-only in check (design §8)
		return i;
	}

	void Start() override {
		units.assign(unitHandler.MaxUnits(), Unit{});
		srcs.assign(unitHandler.MaxUnits(), Src{});
		slots.clear();
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Init", LInit);
		LuaPushRawNamedCFunc(L, "Source", LSource);
		LuaPushRawNamedCFunc(L, "UnitAnswers", LUnitAnswers);
		LuaPushRawNamedCFunc(L, "Reset", LReset);
		LuaPushRawNamedCFunc(L, "Slot", LSlot);
		LuaPushRawNamedCFunc(L, "Fresh", LFresh);
		LuaPushRawNamedCFunc(L, "Unsettle", LUnsettle);
		LuaPushRawNamedCFunc(L, "Run", LRun);
		LuaPushRawNamedCFunc(L, "Fact", LFact);
		LuaPushRawNamedCFunc(L, "Circle", LCircle);
		LuaPushRawNamedCFunc(L, "SensorCircle", LSensorCircle);
	}

private:
	static IntelFacts* Self(lua_State* L) {
		IntelFacts* r = static_cast<IntelFacts*>(Registry::Get().Find("IntelFacts"));
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}
	bool ValidId(int u) const { return u >= 0 && u < (int)units.size(); }
	static bool Flag(const std::vector<uint8_t>& v, int d) { return d >= 0 && (size_t)d < v.size() && v[d] != 0; }

	// ---- circles (the index writes; never in shadow) ----
	void FreeSens(Sens& c, int k, int u) {
		if (c.pc.a == NONE) return;
		if (c.pc.placed) SupComIntIndex::FreeC(SENS_NAME[k], c.pc.a, u);
		if (c.trail) SupComIntIndex::FreeC("radar_leave", c.pc.a, u);
		c.pc = Placed{}; c.trail = false;
	}
	void FreeVision(Src& s, int u) {
		if (s.slotA == NONE) return;
		if (!lastShadow) { SupComIntIndex::FreeC("vision", s.slotA, u); SupComIntIndex::FreeC("vision_leave", s.slotA, u); }
		s.slotA = NONE; s.trail = false;
	}
	void FreeAll(Src& s, int u) {
		FreeVision(s, u);
		for (int k = 0; k < NSENS; ++k) FreeSens(s.sens[k], k, u);
	}

	// ---- the pass ----
	struct Read { const CUnit* unit = nullptr; float x = 0, y = 0, z = 0, gh = 0; int fl = 0; };

	// the sensor kinds of one source (F.SrcLua's loop for the four kinds), before vision
	void SensorKinds(Src& s, int u, int d, int a, float x, float z, bool built, bool carried, bool landed, int fl,
	                 float f, int stamp, bool shadow) {
		const bool sShadow = shadow || cfg.sensors == SENS_SHADOW;
		const bool on = (!Flag(cfg.onoff, d) || (fl & FL_ACTIVE) != 0) && !s.upOff;
		const bool air = Flag(cfg.air, d);
		const C::Rules& R = cfg.rules;
		for (int k = 0; k < NSENS; ++k) {
			Sens& c = s.sens[k];
			if (!c.has) continue;
			bool live = !carried && (built || (cfg.unfinished && k == WATER_VISION)) && (on || !SENS_SWITCHED[k]);
			float r = c.r;
			if (SENS_POWER[k]) r = r * s.scale;
			if (k == WATER_VISION && air && landed) r = r * R.landedVision;
			if (k == WATER_VISION) live = live && cfg.waterSeesSub;
			c.hasDr = live && r > 0.0f; c.dr = c.hasDr ? r : 0.0f;
			if (c.pc.a != NONE && (c.pc.a != a || !c.hasDr)) { if (sShadow) { c.pc = Placed{}; c.trail = false; } else FreeSens(c, k, u); }
			if (!c.hasDr) continue;
			const float sc = cfg.sensCell[k];
			float qx, qz, sx, sz;
			C::Stamp(R, c.st, x, z, c.dr * R.geo.MU, f, sc, qx, qz);
			C::Shot(R.steps, c.sh, qx, qz, sc, f, sx, sz);
			const float cE = C::Reach(c.dr, R.geo.MU, sc);
			const float r2 = C::CircleR2(cE, sc);
			const bool same = c.pc.placed && c.pc.a == a && c.pc.x == sx && c.pc.z == sz && c.pc.r2 == r2 && c.pc.ce == cE;
			if (!sShadow && !same) { SupComIntIndex::PutC(SENS_NAME[k], a, u, sx, sz, r2, stamp, cE); c.pc.placed = true; }
			if (k == RADAR && !sShadow) RadarLeave(c, a, u, sx, sz, r2, cE, stamp);
			c.pc.a = a; c.pc.x = sx; c.pc.z = sz; c.pc.r2 = r2; c.pc.ce = cE; c.pc.mark = stamp;
		}
	}

	// radar's leaving point (DESIGN: no current requirement; int_coverage VI.RadarLeave)
	void RadarLeave(Sens& c, int a, int u, float sx, float sz, float r2, float cE, int stamp) {
		if (c.sh.lcx != sx || c.sh.lcz != sz) {
			if (!c.trail || c.pc.a != a || c.tlx != c.sh.lcx || c.tlz != c.sh.lcz || c.tr2 != r2 || c.tce != cE) {
				SupComIntIndex::PutC("radar_leave", a, u, c.sh.lcx, c.sh.lcz, r2, stamp, cE);
				c.trail = true; c.tlx = c.sh.lcx; c.tlz = c.sh.lcz; c.tr2 = r2; c.tce = cE;
			}
		} else if (c.trail) { SupComIntIndex::FreeC("radar_leave", a, u); c.trail = false; }
	}

	// the vision part of one source; returns whether its circle was redrawn (int_prof's shot count)
	bool Vision(Src& s, int u, int a, float x, float z, bool liveV, bool landed, bool air, bool sub, bool all0, float f,
	            int stamp, bool shadow, lua_State* L, int EV, int& nEv) {
		const C::Rules& R = cfg.rules;
		float r = s.vr;
		if (air) {
			if (landed) r = r * R.landedVision;
			const int lv = landed ? 1 : 0;
			if (s.landed != lv) { s.landed = lv; PushEv(L, EV, nEv, u, EV_LANDED, lv); }
		}
		if (sub) r = s.hasW ? s.wr : r;
		s.hasDr = liveV && r > 0.0f; s.dr = s.hasDr ? r : 0.0f;
		if (s.rda != NONE && (s.rda != a || !s.hasDr)) {
			if (s.slotA == s.rda) FreeVision(s, u);
			s.rda = NONE;
		}
		if (!s.hasDr) return false;
		const float cell = R.cell;
		float qx, qz;
		C::Stamp(R, s.st, x, z, s.dr * R.geo.MU, f, cell, qx, qz);
		const float kx = C::Floor(qx / cell), kz = C::Floor(qz / cell);
		const bool k0 = !all0 && s.slotA == a;
		if (k0 && s.sh.hf.size() == 1 && s.sh.hasC && s.sh.c == cell && s.hasRk && s.rkx == kx && s.rkz == kz && s.rdr == s.dr && s.rda == a) {
			s.mark = stamp;                                                    // kept alive
			return false;
		}
		s.hasRk = true; s.rkx = kx; s.rkz = kz; s.rdr = s.dr; s.rda = a;
		float sx, sz;
		C::Shot(R.steps, s.sh, qx, qz, cell, f, sx, sz);
		const float cE = C::Reach(s.dr, R.geo.MU, cell);
		const float r2 = C::CircleR2(cE, cell);
		if (s.slotA != NONE && s.slotA != a) FreeVision(s, u);
		if (!shadow) SupComIntIndex::PutC("vision", a, u, sx, sz, r2, stamp, cE);
		s.slotA = a; s.mark = stamp;
		s.trail = s.sh.lcx != sx || s.sh.lcz != sz;
		if (s.trail) {
			s.tlx = s.sh.lcx; s.tlz = s.sh.lcz;
			if (!shadow) SupComIntIndex::PutC("vision_leave", a, u, s.sh.lcx, s.sh.lcz, r2, stamp, cE);
		} else if (!shadow) SupComIntIndex::FreeC("vision_leave", a, u);
		return true;
	}

	struct Counts { int rd = 0, st = 0, sh = 0, fin = 0, drop = 0, ev = 0; };

	// the row back into F (not in shadow): the wide five always, the rest when whole
	void WriteRow(lua_State* L, int i, const C::Row& o, bool luaDef, bool whole) const {
		auto N = [&](Col c, float v) { lua_pushnumber(L, v); lua_rawseti(L, col[c], i); };
		auto B = [&](Col c, bool v) { lua_pushboolean(L, v); lua_rawseti(L, col[c], i); };
		N(C_PX, o.px); N(C_PZ, o.pz); N(C_PY, o.py); N(C_PDEF, (float)o.def); N(C_POWN, (float)o.own);
		if (!luaDef) B(C_PCLOAK, false);                                     // Lua answers the units it finishes
		if (!whole) return;
		N(C_FCX, o.fcx); N(C_FCZ, o.fcz); N(C_CCX, o.ccx); N(C_CCZ, o.ccz); N(C_VX, o.px); N(C_VZ, o.pz);
		N(C_VKEY, o.vkey); N(C_SKEY, o.skey); N(C_FK, o.fk); N(C_CK, o.ck); N(C_FB, o.fb); N(C_CB, o.cb);
		B(C_PSUB, o.sub); B(C_PLAND, o.landed);
	}

	// one candidate slot (the body of SupComIntFactsRun's loop)
	void Candidate(int i, float f, float FG, float ff, int stamp, bool all0, bool shadow, lua_State* L, Counts& n) {
		constexpr int FIN = 8, DROP = 9, EV = 10;                          // DESIGN: Run's argument slots (LRun)
		SlotRec& sr = slots[i];
		const int u = sr.u;
		if (u < 0 || !ValidId(u)) return;                                  // a hole is a unit that is gone
		Unit& un = units[u];
		const bool same = sr.readU == u;
		const bool fresh = un.fresh;
		sr.readU = u;
		const bool rd = C::ReadNow(all0, u, FG, ff, fresh, same);
		if (!rd) return;
		n.rd += 1;
		const CUnit* unit = unitHandler.GetUnit(u);
		if (unit == nullptr) {                                             // gone without a destroy event: F.Drop after the call
			Push2(L, DROP, n.drop, (float)i, (float)u);
			n.st += 1;
			return;
		}
		const float x = unit->pos.x, y = unit->pos.y, z = unit->pos.z;
		Src& s = srcs[u];
		const bool calm = s.present && s.calm;
		const C::Row& o = sr.row;
		if (!all0 && same && !fresh && calm && sr.hasRow && x == o.px && z == o.pz && y == o.py) { n.st += 1; return; }
		const int fl = Flags(unit);
		const float gh = CGround::GetHeightReal(x, z, true);
		C::Row& row = sr.row;
		C::Cells(cfg.rules.geo, x, z, row);
		row.py = y; row.def = unit->unitDef->id; row.own = unit->allyteam;
		row.sub = Submerged(unit, y, fl);
		const bool air = Flag(cfg.air, row.def);
		row.landed = air && C::Landed(cfg.rules, y, gh);
		sr.hasRow = true;
		const bool luaDef = Flag(cfg.lua, row.def);
		const bool sensOn = cfg.sensors == SENS_ON;
		const bool luaFin = luaDef || (s.present && (s.nv || (!s.hasV && !(sensOn && s.anySens))));
		if (!shadow) WriteRow(L, i, row, luaDef, !lean || luaFin || Flag(cfg.full, row.def));
		if (luaFin) Push2(L, FIN, n.fin, (float)i, (float)fl);
		if (!s.present || (!s.hasV && !(cfg.sensors != SENS_LUA && s.anySens))) return;
		SourceOf(s, u, row, fl, air, f, stamp, all0, shadow, L, EV, n);
	}

	void SourceOf(Src& s, int u, const C::Row& row, int fl, bool air, float f, int stamp, bool all0, bool shadow,
	              lua_State* L, int EV, Counts& n) {
		Unit& un = units[u];
		if (!un.built) un.built = (fl & FL_BUILDING) == 0;
		const bool carried = (fl & FL_CARRIED) != 0;
		if (cfg.sensors != SENS_LUA && s.anySens)
			SensorKinds(s, u, row.def, row.own, row.px, row.pz, un.built, carried, row.landed, fl, f, stamp, shadow);
		if (!s.hasV) {                                                     // a sensor-only source: no vision part
			if (cfg.sensors == SENS_ON) { un.nv = true; s.calm = false; }
			return;
		}
		const bool liveV = !carried && (un.built || cfg.unfinished);
		if (Vision(s, u, row.own, row.px, row.pz, liveV, row.landed, air, row.sub, all0, f, stamp, shadow, L, EV, n.ev)) n.sh += 1;
		const int uv = row.sub ? 1 : 0;                                    // the engine's sight radius follows the water radius
		if (s.hasW && liveV && s.under != uv) { s.under = uv; PushEv(L, EV, n.ev, u, EV_UNDER, uv); }
		const bool other = s.nv || s.anySens;
		un.nv = other;
		s.calm = !other && un.built && !carried && !Flag(cfg.cloak, row.def) && s.hasDr && s.st.fresh != 0;
	}

	void Sweep(int stamp) {                                                // all0: circles not written this pass (VI.Sweep)
		for (size_t u = 0; u < srcs.size(); ++u) {
			Src& s = srcs[u];
			if (!s.present) continue;
			if (s.slotA != NONE && s.mark != stamp) FreeVision(s, (int)u);
			if (cfg.sensors != SENS_LUA)
				for (int k = 0; k < NSENS; ++k)
					if (s.sens[k].pc.a != NONE && s.sens[k].pc.mark != stamp) FreeSens(s.sens[k], k, (int)u);
		}
	}

	static int LInit(lua_State* L);
	static int LSource(lua_State* L);
	static int LUnitAnswers(lua_State* L);
	static int LReset(lua_State* L);
	static int LSlot(lua_State* L);
	static int LFresh(lua_State* L);
	static int LUnsettle(lua_State* L);
	static int LRun(lua_State* L);
	static int LFact(lua_State* L);
	static int LCircle(lua_State* L);
	static int LSensorCircle(lua_State* L);

	Cfg cfg;
	std::vector<Unit> units;              // per unit id
	std::vector<Src> srcs;                // per unit id
	std::vector<SlotRec> slots;           // per slot, 1-based as Lua's (slot 0 unused)
	bool lastShadow = false;
	bool lean = false;                    // this Run's lean flag
	int col[C_N] = {};                    // this Run's stack index of each F column
};

int IntelFacts::LInit(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	luaL_checktype(L, 1, LUA_TTABLE);
	Cfg& c = r->cfg;
	C::Geometry& g = c.rules.geo;
	g.fe = Num(L, 1, "fe", 0); g.ce = Num(L, 1, "ce", 0);
	g.fnx = Num(L, 1, "fnx", 0); g.fnz = Num(L, 1, "fnz", 0); g.cnx = Num(L, 1, "cnx", 0); g.cnz = Num(L, 1, "cnz", 0);
	g.FB = Num(L, 1, "FB", g.FB); g.CB = Num(L, 1, "CB", g.CB); g.VS = Num(L, 1, "VS", g.VS); g.SS = Num(L, 1, "SS", g.SS);
	g.MU = Num(L, 1, "MU", g.MU);
	c.rules.cell = Num(L, 1, "cell", 0);
	c.rules.redrawFraction = Num(L, 1, "rf", 0);
	c.rules.restampOnCell = Bool(L, 1, "rc");
	c.rules.landedHeight = Num(L, 1, "landedH", 0); c.rules.landedVision = Num(L, 1, "landedV", 1);
	C::Seconds sec;                                                    // times in seconds (LUA-FREE-PLAN §2.2 p6)
	sec.redrawInterval = Num(L, 1, "redraw_interval", 0);
	sec.moveLagEnter = Num(L, 1, "move_lag_enter", 0); sec.moveLagLeave = Num(L, 1, "move_lag_leave", 0);
	sec.redrawPeriod = Num(L, 1, "redraw_period", sec.redrawPeriod); sec.refTick = Num(L, 1, "ref_tick", sec.refTick);
	sec.factsGridTicks = Num(L, 1, "facts_grid_ticks", sec.factsGridTicks);
	c.rules.steps = C::Steps::From(sec, GAME_SPEED, true, (int32_t)Num(L, 1, "sg", 1));   // the pass runs staggered only (efOK)
	c.unfinished = Bool(L, 1, "unfinished"); c.waterSeesSub = Bool(L, 1, "waterSeesSub");
	c.sensors = (int)Num(L, 1, "sensors", 0);
	for (int k = 0; k < NSENS; ++k) c.sensCell[k] = Num(L, 1, (std::string("cell_") + SENS_NAME[k]).c_str(), 0);
	DefFlags(L, 1, "air", c.air); DefFlags(L, 1, "lua", c.lua); DefFlags(L, 1, "cloak", c.cloak); DefFlags(L, 1, "onoff", c.onoff); DefFlags(L, 1, "full", c.full);
	const bool noHeight = Num(L, 1, "hk", 0) == 0.0f;                    // the production rule has no height term
	c.ok = g.fe > 0.0f && g.ce > 0.0f && c.rules.cell > 0.0f && noHeight;
	if (c.sensors != SENS_LUA) for (int k = 0; k < NSENS; ++k) if (!(c.sensCell[k] > 0.0f)) c.ok = false;
	lua_pushboolean(L, c.ok);
	return 1;
}

int IntelFacts::LSource(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (!r->ValidId(u)) return 0;
	Src& s = r->srcs[u];
	const bool any = !lua_isnoneornil(L, 2) || !lua_isnoneornil(L, 3) || lua_toboolean(L, 4) || lua_istable(L, 5);
	if (!any) {
		if (s.present) r->FreeAll(s, u);
		s = Src{};
		r->units[u].built = false;
		return 0;
	}
	s.present = true;
	s.hasV = lua_isnumber(L, 2); s.vr = s.hasV ? (float)lua_tonumber(L, 2) : 0.0f;
	s.hasW = lua_isnumber(L, 3); s.wr = s.hasW ? (float)lua_tonumber(L, 3) : 0.0f;
	s.nv = lua_toboolean(L, 4);
	s.anySens = false;
	for (int k = 0; k < NSENS; ++k) {
		float rr = 0.0f;
		bool has = false;
		if (lua_istable(L, 5)) {
			lua_getfield(L, 5, SENS_NAME[k]);
			has = lua_isnumber(L, -1);
			if (has) rr = (float)lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
		Sens& c = s.sens[k];
		if (!has && c.has) r->FreeSens(c, k, u);
		c.has = has; c.r = rr;
		s.anySens = s.anySens || has;
	}
	return 0;
}

int IntelFacts::LUnitAnswers(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (!r->ValidId(u) || !r->srcs[u].present) return 0;
	r->srcs[u].upOff = lua_toboolean(L, 2);
	r->srcs[u].scale = (float)luaL_optnumber(L, 3, 1.0);
	return 0;
}

int IntelFacts::LReset(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	for (size_t u = 0; u < r->srcs.size(); ++u) if (r->srcs[u].present) r->FreeAll(r->srcs[u], (int)u);
	return 0;
}

int IntelFacts::LSlot(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int j = luaL_checkint(L, 1);
	if (j < 1) return 0;
	if (j >= (int)r->slots.size()) r->slots.resize(j + 1);
	SlotRec& sr = r->slots[j];
	if (sr.u >= 0 && r->ValidId(sr.u) && r->units[sr.u].slot == j) r->units[sr.u].slot = -1;
	sr.u = lua_isnumber(L, 2) ? lua_toint(L, 2) : -1;
	if (sr.u < 0) { sr.readU = -1; sr.hasRow = false; }                // F.Drop: F.ids[j] = nil
	if (r->ValidId(sr.u)) r->units[sr.u].slot = j;
	return 0;
}

int IntelFacts::LFresh(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (r->ValidId(u)) r->units[u].fresh = lua_toboolean(L, 2);
	return 0;
}

int IntelFacts::LUnsettle(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (r->ValidId(u)) r->srcs[u].calm = false;
	return 0;
}

int IntelFacts::LRun(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	luaL_checktype(L, 1, LUA_TTABLE);
	const int cn = luaL_checkint(L, 2);
	const bool part = lua_toboolean(L, 3);
	const float f = luaL_checkfloat(L, 4);
	const int stamp = luaL_checkint(L, 5);
	const bool all0 = lua_toboolean(L, 6);
	const bool shadow = lua_toboolean(L, 7);
	luaL_checktype(L, 8, LUA_TTABLE); luaL_checktype(L, 9, LUA_TTABLE); luaL_checktype(L, 10, LUA_TTABLE);
	luaL_checktype(L, 11, LUA_TTABLE);
	r->lean = lua_toboolean(L, 12);
	lua_settop(L, 11);
	if (!lua_checkstack(L, C_N + 8)) return luaL_error(L, "KT1.IntelFacts.Run: stack");
	for (int c = 0; c < C_N; ++c) {                                    // F[name], made when absent (as SupComIntFactsRun's Field)
		lua_getfield(L, 11, COL_NAME[c]);
		if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setfield(L, 11, COL_NAME[c]); }
		r->col[c] = lua_gettop(L);
	}
	if (!r->cfg.ok) return luaL_error(L, "KT1.IntelFacts.Run: Init first");
	ScopedTimer t(hashString("KT1::IntelFacts"));
	r->lastShadow = shadow;
	const float FG = (float)r->cfg.rules.steps.FG;
	const float ff = C::Mod(f, FG);
	Counts n;
	for (int ci = 1; ci <= cn; ++ci) {
		int i = ci;
		if (part) { lua_rawgeti(L, 1, ci); i = lua_toint(L, -1); lua_pop(L, 1); }
		if (i >= 1 && i < (int)r->slots.size()) r->Candidate(i, f, FG, ff, stamp, all0, shadow, L, n);
	}
	if (all0 && !shadow) r->Sweep(stamp);
	for (Unit& un : r->units) un.fresh = false;                       // int_coverage clears F.fresh after the pass
	lua_pushnumber(L, n.rd); lua_pushnumber(L, n.st); lua_pushnumber(L, n.sh);
	lua_pushnumber(L, n.fin); lua_pushnumber(L, n.drop); lua_pushnumber(L, n.ev);
	return 6;
}

int IntelFacts::LFact(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (!r->ValidId(u)) return 0;
	if (lua_isstring(L, 2)) {
		const std::string col = lua_tostring(L, 2);
		if (col == "built") { lua_pushboolean(L, r->units[u].built); return 1; }
		if (col != "dr") return luaL_error(L, "KT1.IntelFacts.Fact: unknown column %s", col.c_str());
		const Src& s = r->srcs[u];
		if (!s.present) return 0;
		const std::string kind = luaL_optstring(L, 3, "vision");
		if (kind == "vision") {   // and the stamp's frame (cases)
			if (s.hasDr) lua_pushnumber(L, s.dr); else lua_pushnil(L);
			lua_pushnumber(L, s.st.stamps);
			if (s.st.has) lua_pushnumber(L, s.st.f); else lua_pushnil(L);
			return 3;
		}
		for (int k = 0; k < NSENS; ++k)
			if (kind == SENS_NAME[k]) {
				const Sens& c = s.sens[k];
				if (c.hasDr) lua_pushnumber(L, c.dr); else lua_pushnil(L);
				lua_pushnumber(L, c.st.stamps);
				return 2;
			}
		return 0;
	}
	const int j = r->units[u].slot;
	if (j < 1 || j >= (int)r->slots.size() || !r->slots[j].hasRow) return 0;
	const C::Row& o = r->slots[j].row;
	lua_pushnumber(L, o.def); lua_pushnumber(L, o.own); lua_pushboolean(L, o.sub); lua_pushboolean(L, o.landed);
	lua_pushnumber(L, o.px); lua_pushnumber(L, o.pz);
	lua_pushnumber(L, o.fcx); lua_pushnumber(L, o.fcz); lua_pushnumber(L, o.ccx); lua_pushnumber(L, o.ccz);
	return 10;
}

int IntelFacts::LCircle(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (!r->ValidId(u) || !r->srcs[u].present) return 0;
	const Src& s = r->srcs[u];
	const float cell = r->cfg.rules.cell;
	if (s.slotA == NONE) lua_pushnil(L); else lua_pushnumber(L, s.slotA);
	lua_pushnumber(L, s.sh.scx); lua_pushnumber(L, s.sh.scz);
	const float cE = s.hasRk ? C::Reach(s.rdr, r->cfg.rules.geo.MU, cell) : 0.0f;
	lua_pushnumber(L, C::CircleR2(cE, cell)); lua_pushnumber(L, cE);
	lua_pushboolean(L, s.trail); lua_pushnumber(L, s.tlx); lua_pushnumber(L, s.tlz);
	if (s.hasDr) lua_pushnumber(L, s.dr); else lua_pushnil(L);
	lua_pushboolean(L, s.calm);
	lua_pushnumber(L, s.st.stamps);
	return 11;
}

int IntelFacts::LSensorCircle(lua_State* L) {
	IntelFacts* r = Self(L);
	if (r == nullptr) return 0;
	const int u = luaL_checkint(L, 1);
	if (!r->ValidId(u) || !r->srcs[u].present) return 0;
	const std::string kind = luaL_checkstring(L, 2);
	for (int k = 0; k < NSENS; ++k) {
		if (kind != SENS_NAME[k]) continue;
		const Sens& c = r->srcs[u].sens[k];
		if (c.pc.a == NONE) lua_pushnil(L); else lua_pushnumber(L, c.pc.a);
		lua_pushnumber(L, c.pc.x); lua_pushnumber(L, c.pc.z); lua_pushnumber(L, c.pc.r2); lua_pushnumber(L, c.pc.ce);
		if (c.hasDr) lua_pushnumber(L, c.dr); else lua_pushnil(L);
		lua_pushnumber(L, c.st.stamps);
		return 7;
	}
	return 0;
}

} // namespace

KT1_REGISTER_SYSTEM(IntelFacts, 4000, IntelFacts)

} // namespace KT1
