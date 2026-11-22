/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: unit pass step "intel" (UNIT-PASS-DESIGN step 10, docs/engine/INTEL-STEP10-DESIGN.md). Under the production rule (exact
// distance, int_cpp buckets, no height term) the step decides, for each live unit u and each pushed ally team a that does not
// hold u's owner in its group, now in {none 0, contact 1, seen 2} and alsoContact (seen and a contact), with int_coverage.lua's
// detection code, and emits kind 10 (INT_STATE) only on a change: id u, a = a, b = now + 4 * also. Order: ascending u, then
// ascending a. A unit's first visit emits every state that is not none. Mod option unitpass_intel = "1" or "check".
// Lua pushes, on change only:
//   Spring.SupComIntRules{exact, water_sees_sub, sonar_ships, cell, rate}   exact false (or never pushed): the step does nothing
//   Spring.SupComIntDefs{[defID] = {floater, jam, canMove}}
//   Spring.SupComIntGroup(a, {ally teams of a's group, a included})
//   Spring.SupComIntUnitFlags(u, cloak, fake)                              fake units are skipped (Lua decides them)
// Read: Spring.SupComIntState(u, a) -> now, also, lastChangeFrame (nil when never set).
// Fields are looked up for the target's OWNER ally team (stealth_radar, stealth_sonar, jam_field, cloak_field). Vision and its
// leaving lag (vision_leave, asked only when the previous state was seen, the unit is not cloaked and vision missed) use the
// unit position, which the vision kinds snap to their cell themselves; radar, sonar, omni and water vision use the position.
// Rate: a unit is visited when id % rate == frame % rate, or out of its slice when it is new or changed owner since its last visit.
// When: Lua calls Spring.SupComIntRun() in F.Step, after it has placed this frame's sensor circles (the unit pass runs before
// synced Lua's GameFrame, so deciding there used the previous frame's circles: check mode L9198, 0.8% seen-vs-none). The pass
// step itself only carries the mod option; Run walks the units in ascending id and emits into the same frame's event batch.

#include "SupComUnitPass.h"

#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/SupComIntIndex.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"

namespace {
	struct Rules { bool exact = false, waterSeesSub = false, sonarShips = false; double cell = 0.0; int rate = 1; };
	struct Def { bool floater = false, jam = false, canMove = false; };
	struct UnitRec {
		const CUnit* ptr = nullptr;   // the unit this id held at its last visit (a reused id is a new unit)
		int ally = -1;                // its ally team at the last visit (a change is an owner change)
		bool cloak = false, fake = false;
	};
	struct State { unsigned char now = 0; bool also = false; bool set = false; int frame = -1; };

	Rules rules;
	std::unordered_map<int, Def> defs;
	std::map<int, std::vector<int>> groups;          // ascending a: the emission order over armies
	std::unordered_map<int, UnitRec> recs;
	std::unordered_map<long long, State> states;      // key u * 256 + a

	inline long long SKey(int u, int a) { return (long long)u * 256 + a; }

	bool InGroup(const std::vector<int>& g, int ally) {
		for (int x : g) if (x == ally) return true;
		return false;
	}

	bool IsSub(const CUnit* u) {
		if (const auto* gm = dynamic_cast<const CSupComGroundMoveType*>(u->moveType))
			return std::string(gm->GetLayerName()) == "sub";
		return false;
	}

	void Decide(const CUnit* u, const UnitRec& r, int f) {
		const int owner = u->allyteam;
		const auto di = defs.find(u->unitDef->id);
		const Def d = (di != defs.end()) ? di->second : Def{};
		const double x = u->pos.x, z = u->pos.z;
		const int ownerList[1] = { owner };
		const bool inStR = SupComIntIndex::SeesAny("stealth_radar", ownerList, 1, x, z);
		const bool inStS = SupComIntIndex::SeesAny("stealth_sonar", ownerList, 1, x, z);
		const bool inJam = SupComIntIndex::SeesAny("jam_field", ownerList, 1, x, z);
		const bool inCl = SupComIntIndex::SeesAny("cloak_field", ownerList, 1, x, z);
		const bool sub = IsSub(u), ship = d.floater;
		const bool cloak = r.cloak || inCl;
		bool vok = false, rok = false, sok = false;
		if (!sub || ship) {
			vok = !cloak;
			rok = !inStR && (d.jam || !inJam);
		}
		if (sub || (ship && rules.sonarShips))
			sok = (sub || rules.sonarShips) && !inStS;

		for (const auto& ga : groups) {
			const int a = ga.first;
			const std::vector<int>& g = ga.second;
			if (a == teamHandler.GaiaAllyTeamID())
				continue;                                    // Lua's loop skips Gaia
			if (InGroup(g, owner))
				continue;                                    // an ally's unit (and the army's own): Lua's trivial branch
			const int* gl = g.data();
			const int gn = int(g.size());
			bool seen = SupComIntIndex::SeesAny("omni", gl, gn, x, z)
				|| (sub && !r.cloak && rules.waterSeesSub && SupComIntIndex::SeesAny("water_vision", gl, gn, x, z));
			const bool contact = (rok && SupComIntIndex::SeesAny("radar", gl, gn, x, z))
				|| (sok && SupComIntIndex::SeesAny("sonar", gl, gn, x, z));
			State& s = states[SKey(u->id, a)];
			if (!seen && vok)
				seen = SupComIntIndex::SeesAny("vision", gl, gn, x, z);
			if (!seen && vok && rules.cell > 0.0 && s.set && s.now == 2)
				seen = SupComIntIndex::SeesAny("vision_leave", gl, gn, x, z);
			const unsigned char now = seen ? 2 : (contact ? 1 : 0);
			const bool also = seen && contact;
			if (s.set ? (s.now != now || s.also != also) : (now != 0 || also)) {
				s.now = now; s.also = also; s.frame = f;
				SupComUnitPass::Emit(u->id, 10, float(a), float(now + 4 * (also ? 1 : 0)));
			}
			s.set = true;
		}
	}

	void Step(CUnit* u, const SupComUnitPass::Facts& facts);

	bool Any(const CUnit*) { return true; }
}

namespace {
	void Visit(CUnit* u, int f) {
		UnitRec& r = recs[u->id];
		const bool fresh = (r.ptr != u) || (r.ally != u->allyteam);
		if (r.ptr != u) {                                                // a new unit on this id: its states start over
			const bool cl = (r.ptr == nullptr) ? r.cloak : false;        // flags pushed before its first visit are kept
			const bool fk = (r.ptr == nullptr) ? r.fake : false;
			r = UnitRec{};
			r.cloak = cl; r.fake = fk;
			for (const auto& ga : groups) states.erase(SKey(u->id, ga.first));
		}
		const int rate = rules.rate < 1 ? 1 : rules.rate;
		if (!fresh && rate > 1 && (u->id % rate) != (f % rate))
			return;
		r.ptr = u;
		r.ally = u->allyteam;
		if (r.fake)
			return;
		Decide(u, r, f);
	}

	void Step(CUnit*, const SupComUnitPass::Facts&) {}   // the option holder; the work is Run, called by Lua

	int Run(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		if (!SupComUnitPass::IsOn("intel") || !rules.exact) return 0;
		const int f = gs->frameNum;
		const int maxID = int(unitHandler.MaxUnits());
		for (int id = 0; id < maxID; ++id) {                         // ascending id: the same order on every client
			CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr || u->isDead)
				continue;
			Visit(u, f);
		}
		return 0;
	}

	bool Bool(lua_State* L, int t, const char* k) {
		lua_getfield(L, t, k);
		const bool b = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return b;
	}

	int PushRules(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		rules.exact = Bool(L, 1, "exact");
		rules.waterSeesSub = Bool(L, 1, "water_sees_sub");
		rules.sonarShips = Bool(L, 1, "sonar_ships");
		lua_getfield(L, 1, "cell"); rules.cell = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0; lua_pop(L, 1);
		lua_getfield(L, 1, "rate"); rules.rate = lua_isnumber(L, -1) ? lua_toint(L, -1) : 1; lua_pop(L, 1);
		return 0;
	}

	int PushDefs(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		defs.clear();
		for (lua_pushnil(L); lua_next(L, 1) != 0; lua_pop(L, 1)) {      // order free: a map from def id
			if (!lua_isnumber(L, -2) || !lua_istable(L, -1)) continue;
			const int id = lua_toint(L, -2);
			const int t = lua_gettop(L);
			defs[id] = Def{ Bool(L, t, "floater"), Bool(L, t, "jam"), Bool(L, t, "canMove") };
		}
		return 0;
	}

	int PushGroup(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		const int a = luaL_checkint(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		std::vector<int> g;
		const int n = int(lua_objlen(L, 2));
		for (int i = 1; i <= n; ++i) { lua_rawgeti(L, 2, i); g.push_back(lua_toint(L, -1)); lua_pop(L, 1); }
		groups[a] = g;
		return 0;
	}

	int PushUnitFlags(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		UnitRec& r = recs[luaL_checkint(L, 1)];
		r.cloak = lua_toboolean(L, 2);
		r.fake = lua_toboolean(L, 3);
		return 0;
	}

	int ReadState(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		const auto it = states.find(SKey(luaL_checkint(L, 1), luaL_checkint(L, 2)));
		if (it == states.end() || !it->second.set) return 0;
		lua_pushnumber(L, it->second.now);
		lua_pushboolean(L, it->second.also);
		lua_pushnumber(L, it->second.frame);
		return 3;
	}
}

namespace SupComUnitPass {
	void RegisterIntelStep() { Register("intel", Any, Step, 1); }

	void PushIntel(lua_State* L) {
		LuaPushRawNamedCFunc(L, "SupComIntRules", PushRules);
		LuaPushRawNamedCFunc(L, "SupComIntDefs", PushDefs);
		LuaPushRawNamedCFunc(L, "SupComIntGroup", PushGroup);
		LuaPushRawNamedCFunc(L, "SupComIntUnitFlags", PushUnitFlags);
		LuaPushRawNamedCFunc(L, "SupComIntState", ReadState);
		LuaPushRawNamedCFunc(L, "SupComIntRun", Run);
	}
}
