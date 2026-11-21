/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: unit pass step "move" (UNIT-PASS-DESIGN step 6, queue item 7): movement start and stop as events, in place of
// snd_events.lua's velocity polling. A mobile unit (unit def speed > 0, not a building) whose speed (elmos per frame, as
// GetUnitVelocity's fourth value) crosses the threshold emits kind 1 (started) or kind 2 (stopped), a = its speed.
// Mod option unitpass_move = "1". Threshold: Spring.SupComUnitPassSetMoveSpeed(elmosPerFrame), default 0.3 (sound tuning).

#include "SupComUnitPass.h"

#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

namespace {
	float moveSpeed = 0.3f;

	bool Mobile(const CUnit* u) { return u->unitDef->speed > 0.0f && !u->unitDef->IsBuildingUnit(); }

	void Step(CUnit* u, const SupComUnitPass::Facts&) {
		const bool moving = u->speed.w > moveSpeed;
		if (moving == u->supcomPassMoving)
			return;
		u->supcomPassMoving = moving;
		SupComUnitPass::Emit(u->id, moving ? 1 : 2, u->speed.w, 0.0f);
	}

}

namespace SupComUnitPass {
	void RegisterMoveStep() { Register("move", Mobile, Step, 1); }

	int SetMoveSpeed(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		moveSpeed = luaL_checkfloat(L, 1);
		return 0;
	}
}
