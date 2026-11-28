/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: what a synced phase function is handed (LUA-FREE-PLAN 2.2 principle 7). No engine include.
//
// There is deliberately no camera, drawer, sound or GUI handle here, and a sim system includes no draw header
// (game-build tools/engine/kt1_lint.py). Seconds become steps with Core::Rate (set by Phases.cpp from GAME_SPEED).
//
// Writes. Every write a system makes goes through this context: Write(fn) for sim state, Order for orders, Emit (or
// DisplayEvents, which asks the same gate) for display effects. In check mode the Lua is authoritative (KT1Register.h):
// those calls do nothing and are counted (writes_skipped on the system's [KT1CHECK] total line). A system whose logic
// depends on its own writes having happened models that in its own shadow state.

#ifndef KT1_HOST_SIM_CONTEXT_H
#define KT1_HOST_SIM_CONTEXT_H

#include "Check.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"
#include "Game/KT1/Core/SimHost/Phase.h"
#include "Game/KT1/Core/SimHost/SimEventLog.h"

struct Command;

namespace KT1 {

struct SimContext {
	int frame;
	Core::SimHost::Phase phase;
	Core::SimHost::IdRange ids;           // handle these ids; assume nothing about the others
	const Core::SimHost::EventBatch& events;    // new since this registration's last run (empty without an event mask)
	bool check;                           // the system's switch is "check": compute only

	// Each returns false when the write was skipped (check mode).
	template<class F> bool Write(F&& write) const {
		if (!Check::MayWrite()) return false;
		write();
		return true;
	}
	bool Order(int unitID, const ::Command& command) const;   // as synced engine code gives it; no unit: nothing
	bool Emit(const Core::SimHost::DisplayEvent& e) const;
};

} // namespace KT1

#endif
