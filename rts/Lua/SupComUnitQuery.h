/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: batched read-only unit queries for synced Lua (Kardashev ENGINE-CANDIDATES A1, A2). No game rules; nothing written to
// the simulation. Results are ordered by unit id (A1) or by distance then unit id (A2), so they are the same on every client.

#ifndef SUPCOM_UNIT_QUERY_H
#define SUPCOM_UNIT_QUERY_H

struct lua_State;

namespace SupComUnitQuery {
	// just before eventHandler.GameFrame: the change lists (created, destroyed, moved) since the previous GameFrame
	void OnUnitsUpdated();
	void PushSyncedRead(lua_State* L);
}

#endif
