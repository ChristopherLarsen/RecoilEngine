/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core: Intel facts (LUA-FREE-WORKPACKAGES S06). The pure part of int_coverage's facts pass: a unit's cells and keys,
// its source's stamp (redraw), shot record (detection lag) and reach. No engine include. Adapter: Rules/Intel/KT1IntelFacts.cpp;
// rule doc: game-build docs/engine/rules/intel-facts.md; design: docs/engine/LUA-FREE-S06-DESIGN.md.
//
// Requirements: CMD-010 v6 R7 (cell of a position, an edge belongs to the higher cell), R8 (radius in whole cells), R9 (builds
// follow the cell rule); INT-005 v7 R1, R2 (redraw by distance or interval), R3b (slices by unit id).
// DESIGN (no current requirement): the restamp on crossing a cell edge (`restampOnCell`: CMD-010 v6 R6 says the old disc
// stays, reported for correction, design §1); the lag rounded to whole facts grids.
// Times are seconds in config and steps here, converted once by Steps::From (LUA-FREE-PLAN §2.2 p6).
//
// Bit-identical to the Lua path (check mode compares): the engine's Lua numbers are float, so each expression is the same
// float expression in the same order as int_coverage.lua's VI.Stamp, VI.Shot, VI.Reach and its facts loop. No FMA
// contraction (IntelFactsCore.cpp sets it; a caller of the inline helpers must too).

#ifndef KT1_CORE_INTEL_FACTS_CORE_H
#define KT1_CORE_INTEL_FACTS_CORE_H

#include <cmath>
#include <cstdint>
#include <vector>

namespace KT1::IntelFactsCore {

// The map's cell geometry (int_coverage geo and VI): map units, except MU.
struct Geometry {
	float fe = 0, ce = 0;          // fine and coarse cell edge
	float fnx = 0, fnz = 0;        // fine cells across the map
	float cnx = 0, cnz = 0;        // coarse cells across the map
	float FB = 16, CB = 4;         // cells per fine and per coarse bucket
	float VS = 128, SS = 1024;     // the vision and the sensor circle index cells
	float MU = 8;                  // map units per radius unit (ELMOS_PER_MU)
};

// The rule's times in seconds (game/gamedata int_values.lua, int_lag_values.lua, int_cell_values.lua); float, as Lua holds them.
struct Seconds {
	float redrawInterval = 0.0f;     // INT-005 v7 R1
	float moveLagEnter = 0.0f;       // DESIGN: the age of the place detection uses (move_lag_enter)
	float moveLagLeave = 0.0f;       //   and of the leaving point (move_lag_leave)
	float redrawPeriod = 0.6f;     // the shot grid when not staggered (redraw_period)
	float refTick = 0.1f;          // one reference tick (G0-MOVE ref_tick)
	float factsGridTicks = 4.0f;     // facts refreshed once in this many reference ticks (facts_grid_ticks or detect_grid_ticks)
};

// The same in sim steps, made once per game. sg: the detection slice period in steps (whole ticks).
struct Steps {
	int32_t ri = 0, lagE = 0, lagL = 0, reF = 1, FG = 1;
	static Steps From(const Seconds& s, int32_t stepsPerSecond, bool staggered, int32_t sg);
};

struct Rules {
	Geometry geo;
	Steps steps;
	float cell = 0;                // the vision cell (map units)
	float redrawFraction = 0;      // INT-005 v7 R1; 0: no stamping
	bool restampOnCell = false;    // DESIGN, see the file header
	float landedHeight = 0, landedVision = 1;
};

// One unit's facts (int_coverage F.px .. F.pland): the columns Spring.KT1.Intel.Fact answers.
struct Row {
	float px = 0, pz = 0, py = 0;
	int32_t def = 0, own = 0;
	float fcx = 0, fcz = 0, ccx = 0, ccz = 0;
	float vkey = 0, skey = 0, fk = -1, ck = -1, fb = 0, cb = 0;
	bool sub = false, landed = false;
};

// float helpers as the engine's Lua computes them
inline float Floor(float v) { return std::floor(v); }
inline float Mod(float a, float b) { return a - std::floor(a / b) * b; }            // luai_nummod
inline float Snap(float v, float c) { return (std::floor(v / c) + 0.5f) * c; }       // VI.Snap: the centre of v's cell
inline float Sq(float v) { return v * v; }                                           // Lua's v ^ 2

// CMD-010 v6 R7 and the index keys: the cells and keys of a position, into out (position fields included).
void Cells(const Geometry& g, float x, float z, Row& out);

// An aircraft is landed within landedHeight of the ground, or of the sea surface over water.
inline bool Landed(const Rules& r, float y, float groundY) {
	return y - (groundY > 0.0f ? groundY : 0.0f) <= r.landedHeight * r.geo.MU;
}

// INT-005 v7 R3b and the facts slice: is unit u read this step. all0: every unit; fresh: new or given; same: its slot still
// holds it; ff = step % FG.
inline bool ReadNow(bool all0, int32_t u, float FG, float ff, bool fresh, bool same) {
	return all0 || Mod(static_cast<float>(u), FG) == ff || fresh || !same;
}

// INT-005 v7 R1 (VI.Stamp): where the disc is drawn, into ox, oz. rE: the radius in map units; c: the kind's cell.
struct Stamped {
	bool has = false; float x = 0, z = 0, f = 0, r = 0;
	int32_t fresh = -1;            // -1: never stamped; 1: at its stamp; 0: moved since and not restamped
	int32_t stamps = 0;
};
void Stamp(const Rules& r, Stamped& s, float x, float z, float rE, float f, float c, float& ox, float& oz);

// The shot record (VI.Shot): the place detection uses, lagE steps old (scx, scz), and the leaving point, lagL old (lcx, lcz).
struct Shots {
	std::vector<float> hf, hx, hz;
	bool hasC = false; float c = 0, g = 0, scx = 0, scz = 0, lcx = 0, lcz = 0;
};
void Shot(const Steps& st, Shots& s, float x, float z, float c, float f, float& ox, float& oz);

// CMD-010 v6 R8 (VI.Reach): the radius in whole cells, as a length.
inline float Reach(float radius, float MU, float c) { return std::floor(radius * MU / c + static_cast<float>(1e-9)) * c; }

// The circle's squared reach in the index: the reach plus one cell (the index tests cell centres).
inline float CircleR2(float cE, float c) { return Sq(cE + c); }

} // namespace KT1::IntelFactsCore

#endif
