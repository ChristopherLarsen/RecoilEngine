/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: the intel coverage circle index for synced Lua (int_coverage.lua, plan option 5). It stores circles and answers "does any
// circle of these ally teams cover this point"; every rule (radii, reach, stamps, states) stays in Lua. Doubles throughout, as Lua.

#ifndef SUPCOM_INT_INDEX_H
#define SUPCOM_INT_INDEX_H

struct lua_State;

namespace SupComIntIndex {
	void PushSyncedCtrl(lua_State* L);
}

#endif
