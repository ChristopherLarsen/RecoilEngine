/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: the intel coverage circle index for synced Lua (int_coverage.lua, plan option 5). It stores circles and answers "does any
// circle of these ally teams cover this point"; every rule (radii, reach, stamps, states) stays in Lua. Doubles throughout, as Lua.

#ifndef SUPCOM_INT_INDEX_H
#define SUPCOM_INT_INDEX_H

struct lua_State;

namespace SupComIntIndex {
	void PushSyncedCtrl(lua_State* L);
	/// C++ form of SupComIntSees for the unit pass: does a circle of any of these ally teams in this kind cover (x, z)
	/// (the kind's own cell snap applies); a kind never initialised covers nothing
	bool SeesAny(const char* kind, const int* allyTeams, int n, double x, double z);
}

#endif
