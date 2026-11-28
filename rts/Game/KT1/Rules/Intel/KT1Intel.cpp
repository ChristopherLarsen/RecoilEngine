/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Intel (version 2). The detection loop of int_coverage.lua: for each live unit u and each army a (ally team, not Gaia)
// that does not own u, whether a sees u, has it as a contact, or neither, and whether a seen unit is also a contact (IEV-05).
// The engine sends only the changes; Lua does what a change does (int_coverage.lua F.OnState).
//
// Replaces:   int_coverage.lua's per-unit, per-army detection loop (GameFrame, "Detection of every enemy unit by each army") under
//             the production rule (exact distance, int_cpp buckets, no height term, not the INT-002 v1 water fixture). With this
//             on, Lua walks only the fake units (REQ INT-008: few; Lua decides them) and the events. The source circles, the
//             released raster rule, fakes and everything a change does stay in Lua. Ported from the fork's unit pass step "intel"
//             (rts/Sim/Units/SupComUnitPassIntel.cpp, docs/engine/INTEL-STEP10-DESIGN.md), which decided the same but left the
//             Lua loop walking every (unit, army) pair.
// Switch:     mod option kt1_intel (default off)
// Config:     gamedata/kt1/intel.lua (no tuning values: the rule numbers come from int_values.lua through Rules())
// Unit data:  per def floater, jam, canMove: pushed once by the gadget (Defs), from its own tables
// Timer:      KT1::Intel (each Run() call)
// Lua (synced only):
//   Spring.KT1.Intel.Rules{exact, water_sees_sub, sonar_ships, cell, rate}   exact false: Run decides nothing
//   Spring.KT1.Intel.Defs{[defID] = {floater, jam, canMove}}
//   Spring.KT1.Intel.Group(a, {ally teams of a's group, a included})
//   Spring.KT1.Intel.UnitFlags(u, cloak, fake)                              a fake unit is skipped (Lua decides it)
//   Spring.KT1.Intel.Forget(u)                                              a unit died: its states start over
//   Spring.KT1.Intel.Turn(a)                                                army turns on: until the next call only army a (-1: none) decides
//                                                                           the units visited this frame (a new or given unit: every army);
//                                                                           Turn() or Turn(nil): off, every army decides every visit
//   Spring.KT1.Intel.Run() -> { u, a, code, ... }   the changes this frame; code = now (0 none, 1 contact, 2 seen) + 4 * also
//   Spring.KT1.Intel.State(u, a) -> now, also, frame of the last change (nil when never set)
// Rule, for each target u (ascending id) visited this frame, each pushed army a (ascending), a not Gaia, a not u's ally team:
//   u in a's group (an ally's unit): seen, also a contact (Lua's trivial branch).
//   Otherwise, fields of u's OWNER: stealth_radar, stealth_sonar, jam_field, cloak_field at u; cloak = u's own cloak or the field.
//   Not a sub, or a ship: vision ok = not cloak; radar ok = not in stealth radar and (u jams or not in a jam field).
//   A sub, or a ship with sonar_ships: sonar ok = not in stealth sonar.
//   seen = omni, or (a sub, not self-cloaked, water_sees_sub) water vision; or (vision ok) vision; or (vision ok, cell rule, seen
//   before) vision_leave (the leaving lag). contact = (radar ok and radar) or (sonar ok and sonar) or (radar ok, cell rule, a
//   contact before, radar_leave: radar's leaving lag, INT-005 v8). Every coverage is "any circle of a's group" in the int_cpp
//   index (Lua/SupComIntIndex), as Lua asks it.
//   Visited: id % rate == frame % rate, or out of its slice when it is new or changed owner since its last visit (Lua's fresh).
//   Army turns (INT-005 v8 R3c, owner 2026-11-25): int_coverage sets rate to the frames of one reference tick and calls Turn
//   before each Run with the ally team whose turn the tick is, so each army's states change on its own turn only.
// Determinism: units ascending id, armies ascending; bool answers of the circle index (order free); no thread, clock or hash order.
// Events:     returned by Run(), which int_coverage calls once a frame after it placed the frame's circles.
// Doc:        game-build docs/engine/rules/intel.md

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Lua/SupComIntIndex.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {
namespace {

class Intel : public Rule {
public:
	Intel() : Rule({"Intel", 2, true, "LuaRules/Gadgets/int_coverage.lua (detection loop)", false}) {}

	void Start() override {
		const int n = int(unitHandler.MaxUnits());
		recs.assign(n, UnitRec{});
		turns = false; turn = -1;
		for (auto& p : armies) p.second.st.assign(n, St{});
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Rules", LRules);
		LuaPushRawNamedCFunc(L, "Defs", LDefs);
		LuaPushRawNamedCFunc(L, "Group", LGroup);
		LuaPushRawNamedCFunc(L, "UnitFlags", LUnitFlags);
		LuaPushRawNamedCFunc(L, "Forget", LForget);
		LuaPushRawNamedCFunc(L, "Turn", LTurn);
		LuaPushRawNamedCFunc(L, "Run", LRun);
		LuaPushRawNamedCFunc(L, "State", LState);
	}

private:
	struct RulesT { bool exact = false, waterSeesSub = false, sonarShips = false; float cell = 0.0f; int rate = 1; };
	struct Def { bool floater = false, jam = false; };
	struct UnitRec { bool seen = false; int ally = -1; bool cloak = false, fake = false; };   // seen: visited since it was made
	struct St { unsigned char now = 0; bool also = false, set = false; int frame = -1; };
	struct Army { std::vector<int> group; std::vector<St> st; };

	static Intel* SyncedSelf(lua_State* L) {
		Intel* r = static_cast<Intel*>(Registry::Get().Find("Intel"));
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}

	static bool InGroup(const std::vector<int>& g, int ally) {
		for (const int x : g) if (x == ally) return true;
		return false;
	}

	static bool IsSub(const CUnit* u) {
		if (const auto* gm = dynamic_cast<const CSupComGroundMoveType*>(u->moveType))
			return std::string(gm->GetLayerName()) == "sub";
		return false;
	}

	UnitRec& Rec(int id) {
		if (id >= (int)recs.size()) recs.resize(id + 1);
		return recs[id];
	}
	St& StOf(Army& ar, int id) {
		if (id >= (int)ar.st.size()) ar.st.resize(std::max<size_t>(id + 1, recs.size()));
		return ar.st[id];
	}

	void Decide(const CUnit* u, const UnitRec& r, int f, bool fresh) {
		using SupComIntIndex::SeesAny;
		const int owner = u->allyteam;
		const int d = u->unitDef->id;
		const Def df = (d >= 0 && d < (int)defs.size()) ? defs[d] : Def{};
		const float x = u->pos.x, z = u->pos.z;   // float sim maths (LUA-FREE-PLAN r7); SupComIntIndex widens them exactly
		bool fieldsRead = false, vok = false, rok = false, sok = false;
		const bool sub = IsSub(u);
		for (auto& p : armies) {
			const int a = p.first;
			if (a == gaia || a == owner) continue;                 // Lua's loop: not Gaia's army, not the army's own units
			if (turns && !fresh && a != turn) continue;            // army turns: the others wait for theirs
			Army& ar = p.second;
			const std::vector<int>& g = ar.group;
			unsigned char now;
			bool also;
			St& s = StOf(ar, u->id);
			if (InGroup(g, owner)) {
				now = 2; also = true;                              // an ally's unit
			} else {
				if (!fieldsRead) {                                 // the owner's fields, once per unit
					const int ol[1] = { owner };
					const bool inStR = SeesAny("stealth_radar", ol, 1, x, z);
					const bool inStS = SeesAny("stealth_sonar", ol, 1, x, z);
					const bool inJam = SeesAny("jam_field", ol, 1, x, z);
					const bool inCl = SeesAny("cloak_field", ol, 1, x, z);
					const bool cloak = r.cloak || inCl;
					if (!sub || df.floater) {
						vok = !cloak;
						rok = !inStR && (df.jam || !inJam);
					}
					if (sub || (df.floater && rules.sonarShips))
						sok = !inStS;
					fieldsRead = true;
				}
				const int* gl = g.data();
				const int gn = int(g.size());
				bool seen = SeesAny("omni", gl, gn, x, z)
					|| (sub && !r.cloak && rules.waterSeesSub && SeesAny("water_vision", gl, gn, x, z));
				bool contact = (rok && SeesAny("radar", gl, gn, x, z)) || (sok && SeesAny("sonar", gl, gn, x, z));
				if (!contact && rok && rules.cell > 0.0f && s.set && (s.now == 1 || s.also))
					contact = SeesAny("radar_leave", gl, gn, x, z);      // radar's leaving lag
				if (!seen && vok)
					seen = SeesAny("vision", gl, gn, x, z);
				if (!seen && vok && rules.cell > 0.0f && s.set && s.now == 2)
					seen = SeesAny("vision_leave", gl, gn, x, z);
				now = seen ? 2 : (contact ? 1 : 0);
				also = seen && contact;
			}
			if (!s.set || s.now != now || s.also != also) {   // a first visit sends every army, none too (Lua sets the LOS mask then)
				s.now = now; s.also = also; s.frame = f;
				ev.push_back(u->id); ev.push_back(a); ev.push_back(now + 4 * (also ? 1 : 0));
			}
			s.set = true;
		}
	}

	void Run(int f) {
		ev.clear();
		if (!rules.exact) return;
		gaia = teamHandler.GaiaAllyTeamID();
		const int rate = rules.rate < 1 ? 1 : rules.rate;
		ids.clear();
		for (const CUnit* u : unitHandler.GetActiveUnits()) {
			if (u->isDead) continue;
			const UnitRec& r = Rec(u->id);
			if (rate == 1 || (u->id % rate) == (f % rate) || !r.seen || r.ally != u->allyteam) ids.push_back(u->id);
		}
		std::sort(ids.begin(), ids.end());                       // the active list is in synced order, not id order
		for (const int id : ids) {
			const CUnit* u = unitHandler.GetUnit(id);
			UnitRec& r = Rec(id);
			const bool fresh = !r.seen || r.ally != u->allyteam;
			r.seen = true;
			r.ally = u->allyteam;
			if (r.fake) continue;
			Decide(u, r, f, fresh);
		}
	}

	void ForgetUnit(int id) {
		if (id < 0) return;
		Rec(id) = UnitRec{};
		for (auto& p : armies) StOf(p.second, id) = St{};
	}

	static bool Bool(lua_State* L, int t, const char* k) {
		lua_getfield(L, t, k);
		const bool b = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return b;
	}

	static int LRules(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		RulesT& q = r->rules;
		q.exact = Bool(L, 1, "exact");
		q.waterSeesSub = Bool(L, 1, "water_sees_sub");
		q.sonarShips = Bool(L, 1, "sonar_ships");
		lua_getfield(L, 1, "cell"); q.cell = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.0f; lua_pop(L, 1);
		lua_getfield(L, 1, "rate"); q.rate = lua_isnumber(L, -1) ? lua_toint(L, -1) : 1; lua_pop(L, 1);
		return 0;
	}

	static int LDefs(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		r->defs.assign(unitDefHandler->NumUnitDefs() + 1, Def{});
		for (lua_pushnil(L); lua_next(L, 1) != 0; lua_pop(L, 1)) {      // order free: a map from def id
			if (!lua_isnumber(L, -2) || !lua_istable(L, -1)) continue;
			const int id = lua_toint(L, -2);
			if (id < 0 || id >= (int)r->defs.size()) continue;
			const int t = lua_gettop(L);
			r->defs[id] = Def{ Bool(L, t, "floater"), Bool(L, t, "jam") };
		}
		return 0;
	}

	static int LGroup(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		const int a = luaL_checkint(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		Army& ar = r->armies[a];
		ar.group.clear();
		const int n = int(lua_objlen(L, 2));
		for (int i = 1; i <= n; ++i) { lua_rawgeti(L, 2, i); ar.group.push_back(lua_toint(L, -1)); lua_pop(L, 1); }
		if (ar.st.size() < r->recs.size()) ar.st.resize(r->recs.size());
		return 0;
	}

	static int LUnitFlags(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		const int id = luaL_checkint(L, 1);
		if (id < 0) return 0;
		UnitRec& u = r->Rec(id);
		u.cloak = lua_toboolean(L, 2);
		u.fake = lua_toboolean(L, 3);
		return 0;
	}

	static int LForget(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		r->ForgetUnit(luaL_checkint(L, 1));
		return 0;
	}

	static int LTurn(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		r->turns = !lua_isnoneornil(L, 1);
		r->turn = r->turns ? luaL_checkint(L, 1) : -1;
		return 0;
	}

	static int LRun(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		{
			ScopedTimer t(hashString("KT1::Intel"));
			r->Run(gs->frameNum);
		}
		const std::vector<int>& ev = r->ev;
		lua_createtable(L, (int)ev.size(), 0);
		for (size_t i = 0; i < ev.size(); ++i) { lua_pushnumber(L, ev[i]); lua_rawseti(L, -2, (int)i + 1); }
		return 1;
	}

	static int LState(lua_State* L) {
		Intel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		const int u = luaL_checkint(L, 1), a = luaL_checkint(L, 2);
		const auto it = r->armies.find(a);
		if (it == r->armies.end() || u < 0 || u >= (int)it->second.st.size() || !it->second.st[u].set) return 0;
		const St& s = it->second.st[u];
		lua_pushnumber(L, s.now);
		lua_pushboolean(L, s.also);
		lua_pushnumber(L, s.frame);
		return 3;
	}

	RulesT rules;
	int gaia = -1;
	bool turns = false;                 // army turns (Turn); turn: the ally team deciding this frame
	int turn = -1;
	std::vector<Def> defs;              // per def id
	std::map<int, Army> armies;         // ascending a: the event order over armies
	std::vector<UnitRec> recs;          // per unit id
	std::vector<int> ids, ev;           // scratch; this frame's events
};

} // namespace

KT1_REGISTER_SYSTEM(Intel, 800, Intel)

} // namespace KT1
