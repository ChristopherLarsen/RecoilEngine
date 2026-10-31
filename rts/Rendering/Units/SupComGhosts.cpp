/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComGhosts.h"

#include "Lua/LuaUtils.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Units/UnitDrawerData.h"
#include "Rendering/Models/3DModel.hpp"
#include "Sim/Misc/TeamHandler.h"
#include "System/Config/ConfigHandler.h"

#include <algorithm>

CONFIG(bool, SupComLuaDeadGhosts).defaultValue(true).description("supcom: the game may own the ghosts of dead buildings (Spring.SetDeadGhostsByLua); 0 keeps the engine's own removal, as stock Recoil.");

namespace {
	bool luaOwned = false;
	int setting = -1; // -1: not read yet

	bool SettingOn()
	{
		if (setting < 0)
			setting = configHandler->GetBool("SupComLuaDeadGhosts")? 1: 0;

		return (setting != 0);
	}
}

// friend of CUnitDrawerData: the dead ghost lists and their removal
struct SupComGhostsAccess {
	using GhostList = std::vector<GhostSolidObject*>;

	static CUnitDrawerData* Data() { return CUnitDrawer::modelDrawerData; }

	static bool ValidAllyTeam(const CUnitDrawerData* d, int allyTeam) {
		return (d != nullptr && allyTeam >= 0 && allyTeam < static_cast<int>(d->savedData.deadGhostBuildings.size()));
	}

	// the list and index holding the ghost of unitID for allyTeam, or nullptr
	static GhostList* Find(CUnitDrawerData* d, int allyTeam, int unitID, int& index) {
		if (!ValidAllyTeam(d, allyTeam))
			return nullptr;

		for (auto& list: d->savedData.deadGhostBuildings[allyTeam]) {
			for (int i = 0; i < static_cast<int>(list.size()); ++i) {
				if (list[i]->unitID != unitID)
					continue;

				index = i;
				return &list;
			}
		}

		return nullptr;
	}

	static bool Remove(int allyTeam, int unitID) {
		CUnitDrawerData* d = Data();
		int index = -1;
		GhostList* list = Find(d, allyTeam, unitID, index);

		if (list == nullptr)
			return false;

		d->RemoveDeadGhost((*list)[index], *list, index);
		return true;
	}

	static bool Share(int fromAllyTeam, int toAllyTeam, int unitID) {
		CUnitDrawerData* d = Data();
		int index = -1;
		GhostList* from = Find(d, fromAllyTeam, unitID, index);

		if (from == nullptr || !ValidAllyTeam(d, toAllyTeam))
			return false;

		int toIndex = -1;
		if (Find(d, toAllyTeam, unitID, toIndex) != nullptr)
			return false;

		GhostSolidObject* gso = (*from)[index];
		d->savedData.deadGhostBuildings[toAllyTeam][gso->GetModel()->type].push_back(gso);
		gso->IncRef();
		return true;
	}

	static void List(lua_State* L, int allyTeam) {
		CUnitDrawerData* d = Data();
		lua_createtable(L, 0, 0);

		if (!ValidAllyTeam(d, allyTeam))
			return;

		std::vector<int> ids;
		for (const auto& list: d->savedData.deadGhostBuildings[allyTeam]) {
			for (const GhostSolidObject* gso: list) {
				ids.push_back(gso->unitID);
			}
		}

		std::sort(ids.begin(), ids.end());
		for (size_t i = 0; i < ids.size(); ++i) {
			lua_pushnumber(L, ids[i]);
			lua_rawseti(L, -2, i + 1);
		}
	}
};

bool SupComGhosts::LuaOwned()
{
	return (luaOwned && SettingOn());
}

static int SetDeadGhostsByLua(lua_State* L)
{
	luaOwned = luaL_checkboolean(L, 1);
	return 0;
}

static int RemoveDeadGhost(lua_State* L)
{
	const int allyTeam = luaL_checkint(L, 1);
	const int unitID = luaL_checkint(L, 2);

	lua_pushboolean(L, SupComGhosts::LuaOwned() && SupComGhostsAccess::Remove(allyTeam, unitID));
	return 1;
}

static int ShareDeadGhost(lua_State* L)
{
	const int fromAllyTeam = luaL_checkint(L, 1);
	const int toAllyTeam = luaL_checkint(L, 2);
	const int unitID = luaL_checkint(L, 3);

	lua_pushboolean(L, SupComGhosts::LuaOwned() && SupComGhostsAccess::Share(fromAllyTeam, toAllyTeam, unitID));
	return 1;
}

static int GetDeadGhosts(lua_State* L)
{
	SupComGhostsAccess::List(L, luaL_checkint(L, 1));
	return 1;
}

void SupComGhosts::PushSyncedCtrl(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SetDeadGhostsByLua", SetDeadGhostsByLua);
	LuaPushRawNamedCFunc(L, "RemoveDeadGhost", RemoveDeadGhost);
	LuaPushRawNamedCFunc(L, "ShareDeadGhost", ShareDeadGhost);
}

void SupComGhosts::PushSyncedRead(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "GetDeadGhosts", GetDeadGhosts);
}
