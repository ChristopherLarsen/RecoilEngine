/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComHalfRate.h.

#include "SupComHalfRate.h"

#include "Game/GameSetup.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/MoveTypes/AAirMoveType.h"
#include "Sim/MoveTypes/GroundMoveType.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Lua/LuaUtils.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "System/Log/ILog.h"

namespace {
	constexpr int IDLE_FRAMES = 0;    // PRF-002 v1 R2: idle from the tick after an update that shows no change
	constexpr int HIT_FRAMES = 1;     // PRF-002 v1 R4: damage wakes the unit for the next tick

	int enabled = -1;                 // -1 unknown, then 0 or 1 (mod options are fixed for the game)
	int countFrame = -1;
	int gTot = 0, gHalf = 0, aTot = 0, aHalf = 0;

	bool Enabled() {
		if (enabled < 0) {
			const auto& mo = CGameSetup::GetModOptions();
			const auto it = mo.find("supcom_half_rate");
			enabled = (it != mo.end() && it->second == "0") ? 0 : 1;   // on unless the game turns it off
		}
		return enabled == 1;
	}

	void Tally(bool ground, bool half) {
		const int f = gs->frameNum;
		if (f != countFrame) {
			if (countFrame >= 0 && countFrame % 300 == 0)
				LOG("[HALFRATE] f=%d ground %d/%d air %d/%d on half rate", countFrame, gHalf, gTot, aHalf, aTot);
			countFrame = f;
			gTot = gHalf = aTot = aHalf = 0;
		}
		if (ground) { ++gTot; gHalf += half; } else { ++aTot; aHalf += half; }
	}

	bool UnitIdle(CUnit* u) {
		// PRF-002 v1 R2: a change of health (damage or regeneration) is a change: awake on the next tick
		const bool sameHealth = (u->health == u->supcomLastHealth);
		u->supcomLastHealth = u->health;
		return sameHealth && !u->beingBuilt && !u->isDead && !u->IsStunned() && !u->IsSkidding() && u->GetTransporter() == nullptr
			&& u->commandAI != nullptr && u->commandAI->commandQue.empty()
			&& u->lastAttackFrame < gs->frameNum - HIT_FRAMES;
	}

	// the run of idle frames, kept in the unit; true when the unit is on half rate this frame
	bool Half(CUnit* u, bool idle) {
		if (!idle) { u->supcomIdleRun = 0; return false; }
		if (u->supcomIdleRun < IDLE_FRAMES) { ++u->supcomIdleRun; return false; }
		return true;
	}
}

bool SupComHalfRate::SkipGround(CUnit* u)
{
	if (!Enabled())
		return false;
	const CGroundMoveType* mt = static_cast<const CGroundMoveType*>(u->moveType);
	const bool idle = UnitIdle(u) && u->speed.w == 0.0f && mt->SupComQuiet();
	const bool half = Half(u, idle);
	Tally(true, half);
	const bool skip = half && ((u->id + gs->frameNum) & 1) != 0;
	if (!skip) u->supcomMoveFrame = gs->frameNum;
	return skip;
}


extern AAirMoveType::GetGroundHeightFunc amtGetGroundHeightFuncs[6];   // AAirMoveType.cpp

bool SupComHalfRate::LandedRest(CUnit* u)
{
	if (!Enabled() || u->beingBuilt)
		return false;
	const bool rest = UnitIdle(u) && u->speed.x == 0.0f && u->speed.y == 0.0f && u->speed.z == 0.0f
		&& u->pos.y == amtGetGroundHeightFuncs[u->unitDef->canSubmerge](u->pos.x, u->pos.z);   // the height UpdateLanded settles to
	const bool half = Half(u, rest);
	Tally(false, half);
	const bool skip = half && ((u->id + gs->frameNum) & 1) != 0;   // PRF-002 v1 R1: updated when tick + id is even
	if (!skip) u->supcomMoveFrame = gs->frameNum;
	return skip;
}

void SupComHalfRate::Moved(CUnit* u) { u->supcomMoveFrame = gs->frameNum; }

// Spring.SupComHalfRateInfo(unitID) -> lastMoveFrame, idleRun   (synced read; nil for an invalid unit; PRF-002 cases)
static int HalfRateInfo(lua_State* L)
{
	const CUnit* u = unitHandler.GetUnit(luaL_checkint(L, 1));
	if (u == nullptr)
		return 0;
	lua_pushnumber(L, u->supcomMoveFrame);
	lua_pushnumber(L, u->supcomIdleRun);
	return 2;
}

void SupComHalfRate::PushSyncedRead(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SupComHalfRateInfo", HalfRateInfo);
}
