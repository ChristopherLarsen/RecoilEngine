/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_GHOSTS_H
#define SUPCOM_GHOSTS_H

struct lua_State;

// supcom: springsetting SupComLuaDeadGhosts (default 1; 0 is stock). The game can own the ghosts of dead buildings
// (synced Lua): after Spring.SetDeadGhostsByLua(true) the engine no longer drops a dead ghost when its LOS covers the
// spot (CUnitDrawerData::UpdateGhostedBuildings); the game removes it with Spring.RemoveDeadGhost(allyTeam, unitID)
// in the same frame its own memory drops the record, and can give an allyTeam the ghost another one has
// (Spring.ShareDeadGhost). Spring.GetDeadGhosts(allyTeam) lists the dead units with a ghost. Ghosts are drawn state
// only: the simulation does not read them. The opt-in is not saved: the game calls it again after a load (gadget
// Initialize). Without a unit drawer (headless) there are no dead ghosts and the calls do nothing.
namespace SupComGhosts {
	/// the game owns the dead ghosts (and the setting is on)
	bool LuaOwned();

	/// LuaSyncedCtrl: SetDeadGhostsByLua, RemoveDeadGhost, ShareDeadGhost
	void PushSyncedCtrl(lua_State* L);
	/// LuaSyncedRead: GetDeadGhosts
	void PushSyncedRead(lua_State* L);
}

#endif
