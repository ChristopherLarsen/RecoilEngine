/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComHalfRate.h.

#include "SupComHalfRate.h"

#include "Game/GameSetup.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/MoveTypes/GroundMoveType.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "System/Log/ILog.h"

namespace {
	constexpr int IDLE_FRAMES = 30;   // DESIGN: frames idle before half rate (1 s)
	constexpr int HIT_FRAMES = 30;    // DESIGN: frames after damage on every frame

	int enabled = -1;                 // -1 unknown, then 0 or 1 (mod options are fixed for the game)
	int countFrame = -1;
	int gTot = 0, gHalf = 0, aTot = 0, aHalf = 0;

	bool Enabled() {
		if (enabled < 0) {
			const auto& mo = CGameSetup::GetModOptions();
			const auto it = mo.find("supcom_half_rate");
			enabled = (it != mo.end() && it->second == "1") ? 1 : 0;
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

	bool UnitIdle(const CUnit* u) {
		return !u->beingBuilt && !u->isDead && !u->IsStunned() && !u->IsSkidding() && u->GetTransporter() == nullptr
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
	return half && ((u->id + gs->frameNum) & 1) != 0;
}

int SupComHalfRate::Air(CUnit* u, bool idleHere)
{
	if (!Enabled())
		return 0;
	const bool half = Half(u, idleHere && UnitIdle(u));
	Tally(false, half);
	if (!half)
		return 0;
	return ((u->id + gs->frameNum) & 1) != 0 ? 1 : 2;
}
