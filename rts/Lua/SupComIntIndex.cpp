/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComIntIndex.h. Synced handles only (others get nothing).
//   Spring.SupComIntInit(kind, S [, cell])        bucket size S (elmos); cell: the cell rule size (nil: exact distance)
//   Spring.SupComIntPut(kind, a, id, x, z, r2, stamp [, ce])
//       circle of unit id for ally team a; r2: the bucket box radius squared (and the exact test d2 <= r2 when no cell);
//       ce: reach under the cell rule (covered when ce > 0 and d2 < ce * ce at the target cell centre)
//   Spring.SupComIntFree(kind, a, id)
//   Spring.SupComIntSweep(kind, a, stamp [, mod, rem])   drops circles whose last Put stamp differs (stamp -1: all);
//       with mod, only circles whose id % mod == rem (Lua's floored %)
//   Spring.SupComIntClear([kind])                 everything of kind (nil: all kinds)
//   Spring.SupComIntSees(kind, allyList, x, z) -> bool
//   Spring.SupComIntSeesMany(kind, allyList, xs, zs, n [, out]) -> out   out[i] = bool for (xs[i], zs[i])
//   Spring.SupComIntFacts(ids, n, ox, oy, oz, ogh, oflags) -> n   read-only facts of units ids[1..n] into the tables given:
//       position (as GetUnitPosition), ground height at (x, z) (as GetGroundHeight), flags 1 carried, 2 active (activated),
//       4 being built, 8 cloaked (isCloaked), 16 movement layer "sub" (supcomLayer); an invalid id gets nil in every table
// Any-hit answers do not depend on the order circles were stored in, so results equal the Lua index (VSees, height term 0).

#include "SupComIntIndex.h"
#include "SupComIntFacts.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "LuaHandle.h"
#include "LuaUtils.h"
#include "Map/Ground.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"

namespace {
	struct Circle { double x, z, r2, ce; int stamp; long long gx0, gx1, gz0, gz1; };
	struct Team {
		std::unordered_map<int, Circle> circles;
		std::unordered_map<long long, std::vector<int>> buckets;
	};
	struct Kind { double S = 128.0; double cell = 0.0; std::unordered_map<int, Team> teams; };
	std::unordered_map<std::string, Kind> kinds;

	inline long long Key(long long gx, long long gz) { return gx * 8192 + gz; }

	void Unhook(Team& t, int id, const Circle& c) {
		for (long long gx = c.gx0; gx <= c.gx1; ++gx)
			for (long long gz = c.gz0; gz <= c.gz1; ++gz) {
				auto it = t.buckets.find(Key(gx, gz));
				if (it == t.buckets.end()) continue;
				auto& v = it->second;
				for (size_t i = 0; i < v.size(); ++i) if (v[i] == id) { v[i] = v.back(); v.pop_back(); break; }
				if (v.empty()) t.buckets.erase(it);
			}
	}

	Kind* GetKind(lua_State* L, int idx, bool create) {
		const std::string k = luaL_checkstring(L, idx);
		auto it = kinds.find(k);
		if (it != kinds.end()) return &it->second;
		return create ? &kinds[k] : nullptr;
	}

	bool Synced(lua_State* L) { return CLuaHandle::GetHandleSynced(L); }

	int Init(lua_State* L) {
		if (!Synced(L)) return 0;
		Kind* k = GetKind(L, 1, true);
		const double S = luaL_checknumber(L, 2);
		const double cell = lua_isnumber(L, 3) ? lua_tonumber(L, 3) : 0.0;
		if (S != k->S) for (auto& p : k->teams) { p.second.circles.clear(); p.second.buckets.clear(); }   // boxes depend on S
		k->S = S;
		k->cell = cell;
		return 0;
	}

	void PutK(Kind* k, int a, int id, double x, double z, double r2, int stamp, double ce) {
		Team& t = k->teams[a];
		Circle c;
		c.x = x; c.z = z; c.r2 = r2; c.stamp = stamp; c.ce = ce;
		const double r = std::sqrt(c.r2), S = k->S;
		c.gx0 = (long long)std::floor((c.x - r) / S); c.gx1 = (long long)std::floor((c.x + r) / S);
		c.gz0 = (long long)std::floor((c.z - r) / S); c.gz1 = (long long)std::floor((c.z + r) / S);
		auto it = t.circles.find(id);
		if (it != t.circles.end()) {
			const Circle o = it->second;
			if (o.gx0 == c.gx0 && o.gx1 == c.gx1 && o.gz0 == c.gz0 && o.gz1 == c.gz1) { it->second = c; return; }
			Unhook(t, id, o);
		}
		t.circles[id] = c;
		for (long long gx = c.gx0; gx <= c.gx1; ++gx)
			for (long long gz = c.gz0; gz <= c.gz1; ++gz) t.buckets[Key(gx, gz)].push_back(id);
	}

	int Put(lua_State* L) {
		if (!Synced(L)) return 0;
		Kind* k = GetKind(L, 1, true);
		const int a = luaL_checkint(L, 2), id = luaL_checkint(L, 3);
		const double x = luaL_checknumber(L, 4), z = luaL_checknumber(L, 5), r2 = luaL_checknumber(L, 6);
		const int stamp = luaL_checkint(L, 7);
		const double ce = lua_isnumber(L, 8) ? lua_tonumber(L, 8) : 0.0;
		PutK(k, a, id, x, z, r2, stamp, ce);
		return 0;
	}

	void FreeK(Kind* k, int a, int id) {
		auto ti = k->teams.find(a);
		if (ti == k->teams.end()) return;
		auto it = ti->second.circles.find(id);
		if (it == ti->second.circles.end()) return;
		Unhook(ti->second, id, it->second);
		ti->second.circles.erase(it);
	}

	int Free(lua_State* L) {
		if (!Synced(L)) return 0;
		Kind* k = GetKind(L, 1, false);
		if (k == nullptr) return 0;
		FreeK(k, luaL_checkint(L, 2), luaL_checkint(L, 3));
		return 0;
	}

	int Sweep(lua_State* L) {
		if (!Synced(L)) return 0;
		Kind* k = GetKind(L, 1, false);
		if (k == nullptr) return 0;
		auto ti = k->teams.find(luaL_checkint(L, 2));
		if (ti == k->teams.end()) return 0;
		const int stamp = luaL_checkint(L, 3);
		Team& t = ti->second;
		const bool slice = lua_isnumber(L, 4);
		const long long mod = slice ? (long long)lua_tonumber(L, 4) : 0, rem = slice ? (long long)lua_tonumber(L, 5) : 0;
		if (stamp == -1 && !slice) { t.circles.clear(); t.buckets.clear(); return 0; }
		for (auto it = t.circles.begin(); it != t.circles.end(); ) {
			const bool inSlice = !slice || mod == 0 || ((((long long)it->first % mod) + mod) % mod) == rem;   // Lua's % (floored)
			if (inSlice && (stamp == -1 || it->second.stamp != stamp)) { Unhook(t, it->first, it->second); it = t.circles.erase(it); }
			else ++it;
		}
		return 0;
	}

	int Clear(lua_State* L) {
		if (!Synced(L)) return 0;
		if (lua_isstring(L, 1)) kinds.erase(lua_tostring(L, 1)); else kinds.clear();
		return 0;
	}

	bool SeesOne(const Kind& k, const std::vector<const Team*>& teams, double x, double z) {
		const double c = k.cell;
		if (c > 0.0) { x = (std::floor(x / c) + 0.5) * c; z = (std::floor(z / c) + 0.5) * c; }
		const long long key = Key((long long)std::floor(x / k.S), (long long)std::floor(z / k.S));
		for (const Team* t : teams) {
			auto bi = t->buckets.find(key);
			if (bi == t->buckets.end()) continue;
			for (int id : bi->second) {
				const Circle& cc = t->circles.at(id);
				const double dx = x - cc.x, dz = z - cc.z, d2 = dx * dx + dz * dz;
				if (c > 0.0) { if (cc.ce > 0.0 && d2 < cc.ce * cc.ce) return true; }
				else if (d2 <= cc.r2) return true;
			}
		}
		return false;
	}

	std::vector<const Team*> listTeams;
	void Teams(lua_State* L, const Kind* k, int idx) {
		listTeams.clear();
		luaL_checktype(L, idx, LUA_TTABLE);
		if (k == nullptr) return;
		const int n = (int)lua_objlen(L, idx);
		for (int i = 1; i <= n; ++i) {
			lua_rawgeti(L, idx, i);
			const int a = lua_toint(L, -1);
			lua_pop(L, 1);
			auto ti = k->teams.find(a);
			if (ti != k->teams.end()) listTeams.push_back(&ti->second);
		}
	}

	int Sees(lua_State* L) {
		if (!Synced(L)) return 0;
		const Kind* k = GetKind(L, 1, false);
		Teams(L, k, 2);
		const double x = luaL_checknumber(L, 3), z = luaL_checknumber(L, 4);
		lua_pushboolean(L, k != nullptr && SeesOne(*k, listTeams, x, z));
		return 1;
	}

	int SeesMany(lua_State* L) {
		if (!Synced(L)) return 0;
		const Kind* k = GetKind(L, 1, false);
		Teams(L, k, 2);
		luaL_checktype(L, 3, LUA_TTABLE);
		luaL_checktype(L, 4, LUA_TTABLE);
		const int n = luaL_checkint(L, 5);
		if (!lua_istable(L, 6)) { lua_settop(L, 5); lua_createtable(L, n, 0); }
		for (int i = 1; i <= n; ++i) {
			lua_rawgeti(L, 3, i); const double x = lua_tonumber(L, -1);
			lua_rawgeti(L, 4, i); const double z = lua_tonumber(L, -1);
			lua_pop(L, 2);
			lua_pushboolean(L, k != nullptr && SeesOne(*k, listTeams, x, z));
			lua_rawseti(L, 6, i);
		}
		lua_pushvalue(L, 6);
		return 1;
	}
}

namespace {
	int Facts(lua_State* L) {
		if (!Synced(L)) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		const int n = luaL_checkint(L, 2);
		for (int t = 3; t <= 7; ++t) luaL_checktype(L, t, LUA_TTABLE);
		for (int i = 1; i <= n; ++i) {
			lua_rawgeti(L, 1, i);
			const int id = lua_toint(L, -1);
			lua_pop(L, 1);
			const CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr) {
				for (int t = 3; t <= 7; ++t) { lua_pushnil(L); lua_rawseti(L, t, i); }
				continue;
			}
			const float3& p = u->pos;
			int flags = 0;
			if (u->GetTransporter() != nullptr) flags |= 1;
			if (u->activated) flags |= 2;
			if (u->beingBuilt) flags |= 4;
			if (u->isCloaked) flags |= 8;
			if (const auto* gm = dynamic_cast<const CSupComGroundMoveType*>(u->moveType)) {
				if (std::string(gm->GetLayerName()) == "sub") flags |= 16;
			}
			lua_pushnumber(L, p.x); lua_rawseti(L, 3, i);
			lua_pushnumber(L, p.y); lua_rawseti(L, 4, i);
			lua_pushnumber(L, p.z); lua_rawseti(L, 5, i);
			lua_pushnumber(L, CGround::GetHeightReal(p.x, p.z, true)); lua_rawseti(L, 6, i);
			lua_pushnumber(L, flags); lua_rawseti(L, 7, i);
		}
		lua_pushnumber(L, n);
		return 1;
	}
}

bool SupComIntIndex::SeesAny(const char* kind, const int* allyTeams, int n, double x, double z)
{
	const auto ki = kinds.find(kind);
	if (ki == kinds.end()) return false;
	static std::vector<const Team*> ts;
	ts.clear();
	for (int i = 0; i < n; ++i) {
		const auto ti = ki->second.teams.find(allyTeams[i]);
		if (ti != ki->second.teams.end()) ts.push_back(&ti->second);
	}
	return !ts.empty() && SeesOne(ki->second, ts, x, z);
}

void SupComIntIndex::PutC(const char* kind, int a, int id, double x, double z, double r2, int stamp, double ce)
{
	PutK(&kinds[kind], a, id, x, z, r2, stamp, ce);   // a kind not yet initialised takes the defaults, as SupComIntPut
}

void SupComIntIndex::FreeC(const char* kind, int a, int id)
{
	const auto ki = kinds.find(kind);
	if (ki != kinds.end()) FreeK(&ki->second, a, id);
}

void SupComIntIndex::PushSyncedCtrl(lua_State* L)
{
	SupComIntFacts::PushSyncedCtrl(L);   // SupComIntFacts*, SupComIntSource (SupComIntFacts.cpp)
	LuaPushRawNamedCFunc(L, "SupComIntFacts", Facts);
	LuaPushRawNamedCFunc(L, "SupComIntInit", Init);
	LuaPushRawNamedCFunc(L, "SupComIntPut", Put);
	LuaPushRawNamedCFunc(L, "SupComIntFree", Free);
	LuaPushRawNamedCFunc(L, "SupComIntSweep", Sweep);
	LuaPushRawNamedCFunc(L, "SupComIntClear", Clear);
	LuaPushRawNamedCFunc(L, "SupComIntSees", Sees);
	LuaPushRawNamedCFunc(L, "SupComIntSeesMany", SeesMany);
}
