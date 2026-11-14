/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComUnitQuery.h.
//   Spring.GetUnitArrays([teamID]) -> ids, xs, ys, zs, defIDs, teams, healths   (flat arrays, ascending unit id)
//   Spring.GetUnitChanges()        -> frame, created, destroyed, moved          (unit ids, ascending; since the previous GameFrame)
//   Spring.GetUnitsInCylinderSorted(x, z, radius [, filter]) -> ids, dists
//     the units Spring.GetUnitsInCylinder(x, z, radius) returns (midPos inside the circle), kept by filter = { team =, allyTeam =,
//     enemyOf = allyTeam, alive = true (not isDead), air = bool (UnitDef IsAirUnit), defs = { [defID] = true }, category = mask
//     (any bit of UnitDef category), maxCount = n }, sorted nearest first by the 2D distance from (x, z) to the unit position
//     (pos, computed in double as Lua would), ties by unit id; dists are those distances
// All answer full-read handles only (synced gadgets); other handles get nil.

#include "SupComUnitQuery.h"

#include <algorithm>
#include <vector>

#include "LuaHandle.h"
#include "LuaUtils.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Units/UnitDefHandler.h"
#include <cmath>
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"

namespace {
	struct Snap { int frame = -1; float3 pos; };

	std::vector<Snap> snaps;            // by unit id: the position at the last unit update (frame -1: absent)
	std::vector<int> created, destroyed, moved;
	int changeFrame = -1;
	int snapFrame = -1;

	void SortedActive(std::vector<const CUnit*>& out)
	{
		out.clear();
		for (const CUnit* u: unitHandler.GetActiveUnits())
			out.push_back(u);
		std::sort(out.begin(), out.end(), [](const CUnit* a, const CUnit* b) { return a->id < b->id; });
	}

	void PushIds(lua_State* L, const std::vector<int>& ids)
	{
		lua_createtable(L, ids.size(), 0);
		for (size_t i = 0; i < ids.size(); i++) {
			lua_pushnumber(L, ids[i]);
			lua_rawseti(L, -2, i + 1);
		}
	}

	int GetUnitArrays(lua_State* L)
	{
		if (!CLuaHandle::GetHandleFullRead(L))
			return 0;

		const int team = luaL_optint(L, 1, -1);
		static std::vector<const CUnit*> us;
		SortedActive(us);
		if (team >= 0)
			us.erase(std::remove_if(us.begin(), us.end(), [&](const CUnit* u) { return u->team != team; }), us.end());

		const int n = us.size();
		for (int f = 0; f < 7; f++)
			lua_createtable(L, n, 0);

		// stack: -7 ids, -6 xs, -5 ys, -4 zs, -3 defIDs, -2 teams, -1 healths
		for (int i = 0; i < n; i++) {
			const CUnit* u = us[i];
			const float vals[7] = { float(u->id), u->pos.x, u->pos.y, u->pos.z, float(u->unitDef->id), float(u->team), u->health };
			for (int f = 0; f < 7; f++) {
				lua_pushnumber(L, vals[f]);
				lua_rawseti(L, -7 + f - 1, i + 1);
			}
		}
		return 7;
	}

	int OptField(lua_State* L, int t, const char* k, int def)
	{
		lua_getfield(L, t, k);
		const int v = lua_isnumber(L, -1)? lua_toint(L, -1): def;
		lua_pop(L, 1);
		return v;
	}

	int GetUnitsInCylinderSorted(lua_State* L)
	{
		if (!CLuaHandle::GetHandleFullRead(L))
			return 0;

		const double x = luaL_checknumber(L, 1), z = luaL_checknumber(L, 2);
		const float fx = x, fz = z, radius = luaL_checkfloat(L, 3), radSqr = radius * radius;

		int team = -1, allyTeam = -1, enemyOf = -1, air = -1, maxCount = -1;
		unsigned int category = 0;
		bool alive = false;
		static std::vector<char> defs;
		bool useDefs = false;

		if (lua_istable(L, 4)) {
			team = OptField(L, 4, "team", -1);
			allyTeam = OptField(L, 4, "allyTeam", -1);
			enemyOf = OptField(L, 4, "enemyOf", -1);
			maxCount = OptField(L, 4, "maxCount", -1);
			category = (unsigned int)OptField(L, 4, "category", 0);
			lua_getfield(L, 4, "alive"); alive = lua_toboolean(L, -1); lua_pop(L, 1);
			lua_getfield(L, 4, "air"); if (lua_isboolean(L, -1)) air = lua_toboolean(L, -1)? 1: 0; lua_pop(L, 1);
			lua_getfield(L, 4, "defs");
			if (lua_istable(L, -1)) {
				useDefs = true;
				defs.assign(unitDefHandler->NumUnitDefs() + 1, 0);
				for (lua_pushnil(L); lua_next(L, -2) != 0; lua_pop(L, 1)) {
					if (!lua_isnumber(L, -2) || !lua_toboolean(L, -1))
						continue;
					const int d = lua_toint(L, -2);
					if (d >= 0 && d < (int)defs.size())
						defs[d] = 1;
				}
			}
			lua_pop(L, 1);
		}

		QuadFieldQuery qfQuery;
		quadField.GetUnitsExact(qfQuery, float3(fx - radius, 0.0f, fz - radius), float3(fx + radius, 0.0f, fz + radius));

		struct Hit { double d; int id; };
		static std::vector<Hit> hits;
		hits.clear();

		for (const CUnit* u: *qfQuery.units) {
			if (u->midPos.SqDistance2D(float3(fx, 0.0f, fz)) > radSqr)
				continue;
			if (team >= 0 && u->team != team) continue;
			if (allyTeam >= 0 && u->allyteam != allyTeam) continue;
			if (enemyOf >= 0 && u->allyteam == enemyOf) continue;
			if (alive && u->isDead) continue;
			if (air >= 0 && (u->unitDef->IsAirUnit()? 1: 0) != air) continue;
			if (category != 0 && (u->unitDef->category & category) == 0) continue;
			if (useDefs && (u->unitDef->id >= (int)defs.size() || !defs[u->unitDef->id])) continue;

			const double dx = x - double(u->pos.x), dz = z - double(u->pos.z);
			hits.push_back({ std::sqrt(dx * dx + dz * dz), u->id });
		}
		std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return (a.d != b.d)? (a.d < b.d): (a.id < b.id); });
		if (maxCount >= 0 && (int)hits.size() > maxCount)
			hits.resize(maxCount);

		lua_createtable(L, hits.size(), 0);
		lua_createtable(L, hits.size(), 0);
		for (size_t i = 0; i < hits.size(); i++) {
			lua_pushnumber(L, hits[i].id); lua_rawseti(L, -3, i + 1);
			lua_pushnumber(L, hits[i].d);  lua_rawseti(L, -2, i + 1);
		}
		return 2;
	}

	int GetUnitChanges(lua_State* L)
	{
		if (!CLuaHandle::GetHandleFullRead(L))
			return 0;

		lua_pushnumber(L, changeFrame);
		PushIds(L, created);
		PushIds(L, destroyed);
		PushIds(L, moved);
		return 4;
	}
}

void SupComUnitQuery::OnUnitsUpdated()
{
	if (snaps.size() != unitHandler.MaxUnits())
		snaps.assign(unitHandler.MaxUnits(), Snap{});

	created.clear();
	destroyed.clear();
	moved.clear();

	static std::vector<const CUnit*> us;
	SortedActive(us);

	const int prev = snapFrame;
	const int now = gs->frameNum;
	size_t k = 0;

	for (int id = 0, n = snaps.size(); id < n; id++) {
		const CUnit* u = (k < us.size() && us[k]->id == id)? us[k++]: nullptr;
		Snap& s = snaps[id];
		const bool was = (s.frame == prev && prev >= 0);

		if (u == nullptr) {
			if (was)
				destroyed.push_back(id);
			s.frame = -1;
			continue;
		}
		if (!was)
			created.push_back(id);
		else if (s.pos.x != u->pos.x || s.pos.y != u->pos.y || s.pos.z != u->pos.z)
			moved.push_back(id);

		s.frame = now;
		s.pos = u->pos;
	}

	snapFrame = now;
	changeFrame = now;
}

void SupComUnitQuery::PushSyncedRead(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "GetUnitArrays", GetUnitArrays);
	LuaPushRawNamedCFunc(L, "GetUnitChanges", GetUnitChanges);
	LuaPushRawNamedCFunc(L, "GetUnitsInCylinderSorted", GetUnitsInCylinderSorted);
}
