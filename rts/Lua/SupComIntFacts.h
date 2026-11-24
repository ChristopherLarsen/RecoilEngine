/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: int_coverage's per-unit facts and vision-source stamping in the engine (game-build docs/engine/INTEL-FACTS-ENGINE-SPEC.md).
// Only SupComIntIndex.cpp includes this (it registers the calls next to the circle index).

#ifndef SUPCOM_INT_FACTS_H
#define SUPCOM_INT_FACTS_H

struct lua_State;

namespace SupComIntFacts {
	void PushSyncedCtrl(lua_State* L);
}

#endif
