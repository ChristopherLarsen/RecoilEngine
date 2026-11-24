/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: int_coverage's facts pass in the engine (game-build docs/engine/INTEL-FACTS-ENGINE-SPEC.md). Synced handles only.
//
// The game's Lua does this per unit in its facts slice: position, cell indices and keys, submerged, landed, then for a vision source the
// stamp (VI.Stamp), the shot record (VI.Shot), the reach (VI.Reach) and the Put into the circle index (SupComIntIndex, kind "vision"),
// with its leaving point (kind "vision_leave"). This file does the same, line for line, under the production rule only (staggered,
// cell rule, int_cpp, no height term, no INT-002 v1 water fixture); Lua keeps everything else (cloak, jamming, the other source kinds).
//
// Bit-identical to the Lua path: the engine's Lua numbers are float (luaconf.h LUA_NUMBER float), so every expression here is the
// same float expression in the same order, with the same streflop math::floor / math::pow, and no FMA contraction (pragma below).
// No thread-dependent state: one synced call per frame, units in the order Lua gives them.
//
//   Spring.SupComIntFactsInit(cfg)               the rule's numbers and per-def flags (cfg fields below); again when they change
//   Spring.SupComIntSource(u [, vr, wr, nv])      unit u's vision radius vr and water vision radius wr (map units; nil: none), nv: it also
//                                                 has a kind other than vision (Lua finishes those); no vr, wr and nv: forget the unit
//   Spring.SupComIntFactsReset()                  frees every circle this file placed (source state kept; Lua's VI.Sweep(a, -1))
//   Spring.SupComIntFactsRun(R, W, cand, cn, part, f, stamp, all0, FG, ff, shadow, fin, drop, ev)
//       -> nRead, nStill, nShot, nFin, nDrop, nEv
//       R: the facts table read (F); W: the one written (F, or a mirror in check mode). cand[1..cn]: slot indices when part, else 1..cn.
//       fin: slots Lua finishes (cloak, jammer, other kinds), with their batch flags (fin[2k-1] slot, fin[2k] flags or -1);
//       drop: (slot, unit) pairs gone without a destroy event (F.Drop); ev: (unit, code, value) triples, code 1 int_landed, 2 under water.
//       shadow: compute only (state of this file evolves; no index writes, writes go to W only).
//   Spring.SupComIntFactsCircle(u) -> a, x, z, r2, ce, trail, lx, lz, dr, calm, stamps   (check mode and cases; nil when no circle)

#pragma GCC optimize("fp-contract=off")

#include "SupComIntFacts.h"
#include "SupComIntIndex.h"

#include <climits>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "LuaHandle.h"
#include "LuaUtils.h"
#include "System/FastMath.h"
#include "Map/Ground.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace {
	constexpr int NONE = INT_MIN;

	struct Cfg {
		float fe = 0, ce = 0, fnx = 0, fnz = 0, cnx = 0, cnz = 0, FB = 16, CB = 4, VS = 128, SS = 1024, MU = 8;
		float cell = 0, rf = 0, ri = 0, lagE = 0, lagL = 0, reF = 1, hk = 0, hclamp = 0, landedH = 0, landedV = 1;
		bool rc = false, unfinished = false, ok = false;
	} cfg;
	std::vector<uint8_t> defAir, defLua, defCloak;   // per def: aircraft; Lua finishes the unit (may cloak, jammer); may cloak

	struct Src {
		bool hasV = false, hasW = false, nv = false;
		float vr = 0, wr = 0;
		bool hasDr = false; float dr = 0;
		bool hasSt = false; float stx = 0, stz = 0, stf = 0, str = 0; int stfresh = -1; int stamps = 0;   // stfresh: -1 nil
		std::vector<float> hf, hx, hz;       // the shot record (VI.Shot), hn = size
		bool hasShc = false; float shc = 0, shg = 0, scx = 0, scz = 0, lcx = 0, lcz = 0;
		bool hasRk = false; float rkx = 0, rkz = 0, rdr = 0; int rda = NONE;
		int landed = -1, under = -1;          // -1 nil
		int slotA = NONE; int mark = 0;       // the ally team its vision circle is placed for (VI.slot[a][u]), its stamp
		bool trail = false;                   // a leaving point is placed (VI.TrailSet)
		float tlx = 0, tlz = 0;
		bool calm = false;
	};
	std::map<int, Src> srcs;                  // ordered: the sweep frees in id order
	std::unordered_set<int> builtC;           // F.built

	bool Synced(lua_State* L) { return CLuaHandle::GetHandleSynced(L); }

	inline float Floor(float v) { return math::floor(v); }
	inline float Mod(float a, float b) { return a - math::floor(a / b) * b; }            // luai_nummod
	inline float Snap(float v, float c) { return (math::floor(v / c) + 0.5f) * c; }       // VI.Snap
	inline float Sq(float v) { return math::pow(v, 2.0f); }                                // Lua's v ^ 2

	float NumField(lua_State* L, int t, const char* k, float def) {
		lua_getfield(L, t, k);
		const float v = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : def;
		lua_pop(L, 1);
		return v;
	}
	bool BoolField(lua_State* L, int t, const char* k) {
		lua_getfield(L, t, k);
		const bool v = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return v;
	}
	void DefFlags(lua_State* L, int t, const char* k, std::vector<uint8_t>& out) {
		out.assign(unitDefHandler->NumUnitDefs() + 1, 0);
		lua_getfield(L, t, k);
		if (lua_istable(L, -1)) {
			for (size_t d = 0; d < out.size(); ++d) {
				lua_rawgeti(L, -1, (int)d);
				out[d] = lua_toboolean(L, -1) ? 1 : 0;
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}

	int Init(lua_State* L) {
		if (!Synced(L)) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		cfg.fe = NumField(L, 1, "fe", 0); cfg.ce = NumField(L, 1, "ce", 0);
		cfg.fnx = NumField(L, 1, "fnx", 0); cfg.fnz = NumField(L, 1, "fnz", 0);
		cfg.cnx = NumField(L, 1, "cnx", 0); cfg.cnz = NumField(L, 1, "cnz", 0);
		cfg.FB = NumField(L, 1, "FB", 16); cfg.CB = NumField(L, 1, "CB", 4);
		cfg.VS = NumField(L, 1, "VS", 128); cfg.SS = NumField(L, 1, "SS", 1024); cfg.MU = NumField(L, 1, "MU", 8);
		cfg.cell = NumField(L, 1, "cell", 0); cfg.rf = NumField(L, 1, "rf", 0); cfg.ri = NumField(L, 1, "ri", 0);
		cfg.lagE = NumField(L, 1, "lagE", 0); cfg.lagL = NumField(L, 1, "lagL", 0); cfg.reF = NumField(L, 1, "reF", 1);
		cfg.hk = NumField(L, 1, "hk", 0); cfg.hclamp = NumField(L, 1, "hclamp", 0);
		cfg.landedH = NumField(L, 1, "landedH", 0); cfg.landedV = NumField(L, 1, "landedV", 1);
		cfg.rc = BoolField(L, 1, "rc"); cfg.unfinished = BoolField(L, 1, "unfinished");
		DefFlags(L, 1, "air", defAir);
		DefFlags(L, 1, "lua", defLua);
		DefFlags(L, 1, "cloak", defCloak);
		cfg.ok = cfg.fe > 0 && cfg.ce > 0 && cfg.cell > 0;
		return 0;
	}

	void FreeCircle(Src& s, int u, bool shadow) {   // VI.Free(a, k): the circle, its leaving point, the trail
		if (s.slotA == NONE) return;
		if (!shadow) {
			SupComIntIndex::FreeC("vision", s.slotA, u);
			SupComIntIndex::FreeC("vision_leave", s.slotA, u);
		}
		s.slotA = NONE;
		s.trail = false;
	}

	bool lastShadow = false;

	int Source(lua_State* L) {
		if (!Synced(L)) return 0;
		const int u = luaL_checkint(L, 1);
		const bool any = !lua_isnoneornil(L, 2) || !lua_isnoneornil(L, 3) || lua_toboolean(L, 4);
		if (!any) {
			auto it = srcs.find(u);
			if (it != srcs.end()) { FreeCircle(it->second, u, lastShadow); srcs.erase(it); }
			builtC.erase(u);
			return 0;
		}
		Src& s = srcs[u];
		s.hasV = lua_isnumber(L, 2); s.vr = s.hasV ? (float)lua_tonumber(L, 2) : 0.0f;
		s.hasW = lua_isnumber(L, 3); s.wr = s.hasW ? (float)lua_tonumber(L, 3) : 0.0f;
		s.nv = lua_toboolean(L, 4);
		return 0;
	}

	int Reset(lua_State* L) {
		if (!Synced(L)) return 0;
		for (auto& p : srcs) FreeCircle(p.second, p.first, lastShadow);
		return 0;
	}

	// VI.Stamp (REQ INT-005 v6 R1, restamp_on_cell)
	void Stamp(Src& s, float x, float z, float rE, float f, float& ox, float& oz) {
		const float fr = cfg.rf;
		if (!(fr > 0.0f)) { ox = x; oz = z; return; }
		if (s.hasSt) {
			const float dx = x - s.stx, dz = z - s.stz, lim = fr * rE;
			const float c = cfg.cell;
			const bool crossed = cfg.rc && (Floor(x / c) != Floor(s.stx / c) || Floor(z / c) != Floor(s.stz / c));
			if (!crossed && dx * dx + dz * dz < lim * lim && f - s.stf < cfg.ri && s.str == rE) {
				s.stfresh = (dx == 0.0f && dz == 0.0f) ? 1 : 0;
				ox = s.stx; oz = s.stz;
				return;
			}
		}
		s.hasSt = true; s.stx = x; s.stz = z; s.stf = f; s.str = rE; s.stfresh = 1;
		s.stamps += 1;
		ox = x; oz = z;
	}

	int At(const Src& s, float t) {          // VI.At, 1-based
		int i = (int)s.hf.size();
		while (i > 1 && s.hf[i - 1] > t) --i;
		return i;
	}

	// VI.Shot (heights not kept: no height term on this path)
	void Shot(Src& s, float x, float z, float c, float f, float reF, float& ox, float& oz) {
		const float g = Floor(f / reF);
		if (s.hf.empty() || !s.hasShc || s.shc != c) {
			s.hf.assign(1, f); s.hx.assign(1, x); s.hz.assign(1, z);
			s.hasShc = true; s.shc = c; s.shg = g;
			s.scx = Snap(x, c); s.scz = Snap(z, c);
			s.lcx = s.scx; s.lcz = s.scz;
			ox = s.scx; oz = s.scz;
			return;
		}
		int n = (int)s.hf.size();
		if (x != s.hx[n - 1] || z != s.hz[n - 1]) {
			s.hf.push_back(f); s.hx.push_back(x); s.hz.push_back(z);
			n += 1;
		}
		if (g != s.shg) {
			s.shg = g;
			const float t = g * reF;
			const int ie = At(s, t - cfg.lagE), il = At(s, t - cfg.lagL);
			s.scx = Snap(s.hx[ie - 1], c); s.scz = Snap(s.hz[ie - 1], c);
			if (il == ie) { s.lcx = s.scx; s.lcz = s.scz; }
			else { s.lcx = Snap(s.hx[il - 1], c); s.lcz = Snap(s.hz[il - 1], c); }
			const int drop = (il < ie ? il : ie) - 1;
			if (drop > 0) {
				s.hf.erase(s.hf.begin(), s.hf.begin() + drop);
				s.hx.erase(s.hx.begin(), s.hx.begin() + drop);
				s.hz.erase(s.hz.begin(), s.hz.begin() + drop);
			}
		}
		ox = s.scx; oz = s.scz;
	}

	inline float Reach(float r, float c) { return Floor(r * cfg.MU / c + (float)1e-9) * c; }   // VI.Reach

	// table helpers (stack index t)
	inline bool GetB(lua_State* L, int t, int k) { lua_rawgeti(L, t, k); const bool v = lua_toboolean(L, -1); lua_pop(L, 1); return v; }
	inline bool GetN(lua_State* L, int t, int k, float& v) {
		lua_rawgeti(L, t, k);
		const bool ok = lua_isnumber(L, -1);
		if (ok) v = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);
		return ok;
	}
	inline void SetN(lua_State* L, int t, int k, float v) { lua_pushnumber(L, v); lua_rawseti(L, t, k); }
	inline void SetB(lua_State* L, int t, int k, bool v) { lua_pushboolean(L, v); lua_rawseti(L, t, k); }
	inline void SetNil(lua_State* L, int t, int k) { lua_pushnil(L); lua_rawseti(L, t, k); }
	inline void SetTrueOrNil(lua_State* L, int t, int k, bool v) { if (v) lua_pushboolean(L, 1); else lua_pushnil(L); lua_rawseti(L, t, k); }

	int Field(lua_State* L, int t, const char* k) {   // pushes t[k] (a table, made when absent), returns its stack index
		lua_getfield(L, t, k);
		if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setfield(L, t, k); }
		return lua_gettop(L);
	}

	int Run(lua_State* L) {
		if (!Synced(L)) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		luaL_checktype(L, 2, LUA_TTABLE);
		luaL_checktype(L, 3, LUA_TTABLE);
		const int cn = luaL_checkint(L, 4);
		const bool part = lua_toboolean(L, 5);
		const float f = luaL_checkfloat(L, 6);
		const int stamp = luaL_checkint(L, 7);
		const bool all0 = lua_toboolean(L, 8);
		const float FG = luaL_checkfloat(L, 9);
		const float ff = luaL_checkfloat(L, 10);
		const bool shadow = lua_toboolean(L, 11);
		luaL_checktype(L, 12, LUA_TTABLE);
		luaL_checktype(L, 13, LUA_TTABLE);
		luaL_checktype(L, 14, LUA_TTABLE);
		lastShadow = shadow;
		if (!lua_checkstack(L, 64)) return luaL_error(L, "SupComIntFactsRun: stack");
		if (!cfg.ok) return luaL_error(L, "SupComIntFactsRun: SupComIntFactsInit first");
		const int CAND = 3, FIN = 12, DROP = 13, EV = 14;

		// read side
		const int R_all = Field(L, 1, "slots"), R_ids = Field(L, 1, "ids"), R_fresh = Field(L, 1, "fresh"), R_calm = Field(L, 1, "calm");
		const int R_mvd = Field(L, 1, "mvd"), R_rdF = Field(L, 1, "rdF"), R_px = Field(L, 1, "px"), R_pz = Field(L, 1, "pz"), R_py = Field(L, 1, "py");
		const int R_sources = Field(L, 1, "sources");
		const float chgFrom = NumField(L, 1, "chgFrom", 0.0f);
		// write side
		const int W_ids = Field(L, 2, "ids"), W_fsf = Field(L, 2, "fsf"), W_px = Field(L, 2, "px"), W_pz = Field(L, 2, "pz"), W_py = Field(L, 2, "py");
		const int W_pdef = Field(L, 2, "pdef"), W_pown = Field(L, 2, "pown"), W_fcx = Field(L, 2, "fcx"), W_fcz = Field(L, 2, "fcz");
		const int W_ccx = Field(L, 2, "ccx"), W_ccz = Field(L, 2, "ccz"), W_vx = Field(L, 2, "vx"), W_vz = Field(L, 2, "vz");
		const int W_vkey = Field(L, 2, "vkey"), W_skey = Field(L, 2, "skey"), W_fk = Field(L, 2, "fk"), W_ck = Field(L, 2, "ck");
		const int W_fb = Field(L, 2, "fb"), W_cb = Field(L, 2, "cb"), W_psub = Field(L, 2, "psub"), W_pland = Field(L, 2, "pland");
		const int W_pcloak = Field(L, 2, "pcloak"), W_calm = Field(L, 2, "calm"), W_nv = Field(L, 2, "nv"), W_built = Field(L, 2, "built");
		const int W_mvd = Field(L, 2, "mvd"), W_rdF = Field(L, 2, "rdF"), W_tch = Field(L, 2, "tch");

		const float fe = cfg.fe, ce = cfg.ce, fnx = cfg.fnx, fnz = cfg.fnz, cnx = cfg.cnx, cnz = cfg.cnz, FB = cfg.FB, CB = cfg.CB;
		const float cell = cfg.cell;
		int nRd = 0, nSt = 0, nSh = 0, nFin = 0, nDrop = 0, nEv = 0;

		for (int ci = 1; ci <= cn; ++ci) {
			int i = ci;
			if (part) { lua_rawgeti(L, CAND, ci); i = lua_toint(L, -1); lua_pop(L, 1); }
			lua_rawgeti(L, R_all, i);
			const bool live = lua_isnumber(L, -1);
			const int u = live ? lua_toint(L, -1) : 0;
			lua_pop(L, 1);
			if (!live) continue;                                                       // a hole is a unit that is gone
			const float uf = (float)u;
			float idv = 0.0f;
			const bool hasId = GetN(L, R_ids, i, idv);
			const bool same = hasId && idv == uf;
			const bool fresh = GetB(L, R_fresh, u);
			const bool rd = all0 || Mod(uf, FG) == ff || fresh || !same;
			SetB(L, W_fsf, i, rd);
			SetN(L, W_ids, i, uf);
			SetN(L, W_tch, i, f);
			bool fsfI = rd;
			if (!rd) continue;
			nRd += 1;
			float opx = 0.0f, opz = 0.0f, opy = 0.0f;
			const bool hasPx = GetN(L, R_px, i, opx);
			GetN(L, R_pz, i, opz);
			const bool hasPy = GetN(L, R_py, i, opy);
			// movers only: a calm unit that has not moved since its last read (int_coverage F.skp)
			bool skp = false;
			if (!all0 && !fresh && same && GetB(L, R_calm, u) && !GetB(L, R_mvd, u) && hasPx) {
				float rdF = -1.0f;
				GetN(L, R_rdF, u, rdF);
				skp = rdF >= chgFrom;
			}
			const CUnit* unit = unitHandler.GetUnit(u);
			bool hasX = false;
			float x = 0, y = 0, z = 0;
			int fl = -1;                                                               // the batch flags (SupComIntFacts), -1: none (skipped)
			float gh = 0.0f;
			if (skp) {
				x = opx; z = opz; y = opy; hasX = true;
				if (!hasPy) return luaL_error(L, "SupComIntFactsRun: py missing for a skipped unit");
			} else if (unit != nullptr) {
				x = unit->pos.x; y = unit->pos.y; z = unit->pos.z; hasX = true;
				fl = 0;
				if (unit->GetTransporter() != nullptr) fl |= 1;
				if (unit->activated) fl |= 2;
				if (unit->beingBuilt) fl |= 4;
				if (unit->isCloaked) fl |= 8;
				if (const auto* gm = dynamic_cast<const CSupComGroundMoveType*>(unit->moveType))
					if (std::string(gm->GetLayerName()) == "sub") fl |= 16;
				gh = CGround::GetHeightReal(x, z, true);
			}
			if (!skp) { SetNil(L, W_mvd, u); SetN(L, W_rdF, u, f); }
			if (!hasX) {                                                               // gone without a destroy event: F.Drop after the call
				nDrop += 1; SetN(L, DROP, 2 * nDrop - 1, (float)i); SetN(L, DROP, 2 * nDrop, uf);
				SetB(L, W_fsf, i, false); nSt += 1;
				continue;
			}
			const bool still = !all0 && same && !fresh && GetB(L, R_calm, u) && hasPx && x == opx && z == opz && hasPy && y == opy;
			if (still) { SetB(L, W_fsf, i, false); nSt += 1; continue; }
			if (unit == nullptr) return luaL_error(L, "SupComIntFactsRun: unit %d gone while calm", u);
			const int d = unit->unitDef->id;
			const int own = unit->allyteam;
			SetN(L, W_px, i, x); SetN(L, W_pz, i, z); SetN(L, W_pdef, i, (float)d); SetN(L, W_pown, i, (float)own);
			const float a1 = Floor(x / fe), b1 = Floor(z / fe), a2 = Floor(x / ce), b2 = Floor(z / ce);
			SetN(L, W_fcx, i, a1); SetN(L, W_fcz, i, b1); SetN(L, W_ccx, i, a2); SetN(L, W_ccz, i, b2);
			const float vx = x, vz = z;                                                // not the req raster: exact position
			SetN(L, W_vx, i, vx); SetN(L, W_vz, i, vz);
			SetN(L, W_vkey, i, Floor(vx / cfg.VS) * 8192.0f + Floor(vz / cfg.VS));
			SetN(L, W_skey, i, Floor(x / cfg.SS) * 8192.0f + Floor(z / cfg.SS));
			SetN(L, W_fk, i, (a1 >= 0 && b1 >= 0 && a1 < fnx && b1 < fnz) ? b1 * fnx + a1 : -1.0f);
			SetN(L, W_ck, i, (a2 >= 0 && b2 >= 0 && a2 < cnx && b2 < cnz) ? b2 * cnx + a2 : -1.0f);
			SetN(L, W_fb, i, Floor(a1 / FB) * 65536.0f + Floor(b1 / FB));
			SetN(L, W_cb, i, Floor(a2 / CB) * 65536.0f + Floor(b2 / CB));
			bool sub = false;
			if (y < 0.0f && fl >= 0 && (fl & 16)) sub = true;
			else if (y < 0.0f) {
				const auto* gm = dynamic_cast<const CSupComGroundMoveType*>(unit->moveType);
				const std::string l = gm != nullptr ? gm->GetLayerName() : "";
				if (l == "sub") sub = true;
				else if (l == "water") sub = false;
				else sub = y + unit->height < 0.0f;
			}
			SetB(L, W_psub, i, sub);
			bool landed = false;
			const bool air = (size_t)d < defAir.size() && defAir[d];
			if (air) {
				const float gy = fl >= 0 ? gh : CGround::GetHeightReal(x, z, true);
				landed = y - (gy > 0.0f ? gy : 0.0f) <= cfg.landedH * cfg.MU;
			}
			SetB(L, W_pland, i, landed);
			const bool luaDef = (size_t)d < defLua.size() && defLua[d];
			if (!luaDef) SetB(L, W_pcloak, i, false);                                  // Lua answers the units it finishes
			const bool mayCloak = (size_t)d < defCloak.size() && defCloak[d];
			SetN(L, W_py, i, y);

			// the source (vision here; Lua finishes the other kinds)
			auto si = srcs.find(u);
			const bool luaFin = luaDef || (si != srcs.end() && (si->second.nv || !si->second.hasV));
			if (luaFin && fsfI) { nFin += 1; SetN(L, FIN, 2 * nFin - 1, (float)i); SetN(L, FIN, 2 * nFin, (float)fl); }
			if (si == srcs.end() || !si->second.hasV) continue;
			Src& s = si->second;
			const int a = own;
			bool built = builtC.count(u) > 0;
			if (!built) {
				built = fl >= 0 ? (fl & 4) == 0 : !unit->beingBuilt;
				if (built) { builtC.insert(u); SetB(L, W_built, u, true); }
			}
			const bool carried = fl >= 0 ? (fl & 1) != 0 : unit->GetTransporter() != nullptr;
			const bool liveV = !carried && (built || cfg.unfinished);
			float r = s.vr;
			if (air) {
				if (landed) r = r * cfg.landedV;
				const int lv = landed ? 1 : 0;
				if (s.landed != lv) { s.landed = lv; nEv += 1; SetN(L, EV, 3 * nEv - 2, uf); SetN(L, EV, 3 * nEv - 1, 1.0f); SetN(L, EV, 3 * nEv, (float)lv); }
			}
			if (sub) r = s.hasW ? s.wr : r;
			s.hasDr = liveV && r > 0.0f; s.dr = s.hasDr ? r : 0.0f;
			if (s.rda != NONE && (s.rda != a || !s.hasDr)) {
				if (s.slotA == s.rda) FreeCircle(s, u, shadow);
				s.rda = NONE;
			}
			if (s.hasDr) {
				const float rE = s.dr * cfg.MU;
				float qx, qz;
				Stamp(s, x, z, rE, f, qx, qz);
				const float kx = Floor(qx / cell), kz = Floor(qz / cell);
				const bool k0 = !all0 && s.slotA == a;
				if (k0 && s.hf.size() == 1 && s.hasShc && s.shc == cell && s.hasRk && s.rkx == kx && s.rkz == kz && s.rdr == s.dr && s.rda == a) {
					s.mark = stamp;                                                    // kept alive
				} else {
					s.hasRk = true; s.rkx = kx; s.rkz = kz; s.rdr = s.dr; s.rda = a;
					nSh += 1;
					float sx, sz;
					Shot(s, qx, qz, cell, f, cfg.reF, sx, sz);
					const float cE = Reach(s.dr, cell);
					const float r2 = Sq(cE + cfg.hk * cfg.hclamp + cell);
					if (s.slotA != NONE && s.slotA != a) FreeCircle(s, u, shadow);
					if (!shadow) SupComIntIndex::PutC("vision", a, u, sx, sz, r2, stamp, cE);
					s.slotA = a; s.mark = stamp;
					const bool tr = s.lcx != sx || s.lcz != sz;
					s.trail = tr;
					if (tr) {
						s.tlx = s.lcx; s.tlz = s.lcz;
						if (!shadow) SupComIntIndex::PutC("vision_leave", a, u, s.lcx, s.lcz, Sq(cE + cell), stamp, cE);
					} else if (!shadow) SupComIntIndex::FreeC("vision_leave", a, u);
				}
			}
			// the source table's radius in use (cases read it), as Lua writes it
			if (!shadow) {
				lua_rawgeti(L, R_sources, u);
				if (lua_istable(L, -1)) {
					lua_getfield(L, -1, "vision");
					if (lua_istable(L, -1)) {
						if (s.hasDr) lua_pushnumber(L, s.dr); else lua_pushnil(L);
						lua_setfield(L, -2, "dr");
						lua_pushnumber(L, (float)s.stamps); lua_setfield(L, -2, "stamps");
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			}
			// the engine's sight radius follows the water radius (Lua: SetUnitSensorRadius)
			const int uv = sub ? 1 : 0;
			if (s.hasW && liveV && s.under != uv) { s.under = uv; nEv += 1; SetN(L, EV, 3 * nEv - 2, uf); SetN(L, EV, 3 * nEv - 1, 2.0f); SetN(L, EV, 3 * nEv, (float)uv); }
			SetTrueOrNil(L, W_nv, u, s.nv);
			s.calm = fsfI && !s.nv && builtC.count(u) > 0 && !carried && !mayCloak && s.hasDr && s.stfresh != 0;
			SetTrueOrNil(L, W_calm, u, s.calm);
		}
		if (all0) {                                                                    // circles not written this pass (VI.Sweep)
			for (auto& p : srcs) if (p.second.slotA != NONE && p.second.mark != stamp) FreeCircle(p.second, p.first, shadow);
		}
		lua_settop(L, 14);
		lua_pushnumber(L, nRd); lua_pushnumber(L, nSt); lua_pushnumber(L, nSh);
		lua_pushnumber(L, nFin); lua_pushnumber(L, nDrop); lua_pushnumber(L, nEv);
		return 6;
	}

	int Circle(lua_State* L) {
		if (!Synced(L)) return 0;
		const auto it = srcs.find(luaL_checkint(L, 1));
		if (it == srcs.end()) return 0;
		const Src& s = it->second;
		if (s.slotA == NONE) lua_pushnil(L); else lua_pushnumber(L, s.slotA);
		lua_pushnumber(L, s.scx); lua_pushnumber(L, s.scz);
		const float cE = s.hasRk ? Reach(s.rdr, cfg.cell) : 0.0f;
		lua_pushnumber(L, Sq(cE + cfg.hk * cfg.hclamp + cfg.cell)); lua_pushnumber(L, cE);
		lua_pushboolean(L, s.trail); lua_pushnumber(L, s.tlx); lua_pushnumber(L, s.tlz);
		if (s.hasDr) lua_pushnumber(L, s.dr); else lua_pushnil(L);
		lua_pushboolean(L, s.calm);
		lua_pushnumber(L, s.stamps);
		return 11;
	}
}

void SupComIntFacts::PushSyncedCtrl(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SupComIntFactsInit", Init);
	LuaPushRawNamedCFunc(L, "SupComIntSource", Source);
	LuaPushRawNamedCFunc(L, "SupComIntFactsReset", Reset);
	LuaPushRawNamedCFunc(L, "SupComIntFactsRun", Run);
	LuaPushRawNamedCFunc(L, "SupComIntFactsCircle", Circle);
}
