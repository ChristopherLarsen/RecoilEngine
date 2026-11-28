/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: what a synced phase function is handed (LUA-FREE-PLAN 2.2 principle 7). No engine include.
//
// There is deliberately no camera, drawer, sound or GUI handle here, and a sim system includes no draw header
// (tools/engine/kt1_simlint.py). A display effect leaves the sim only through DisplayEvents.

#ifndef KT1_HOST_SIM_CONTEXT_H
#define KT1_HOST_SIM_CONTEXT_H

#include "Game/KT1/Core/SimHost/Phase.h"
#include "Game/KT1/Core/SimHost/SimEventLog.h"

namespace KT1 {

struct SimContext {
	int frame;
	int stepsPerSecond;                   // the one sim rate: convert seconds with it, never write a step count
	SimHost::Phase phase;
	SimHost::IdRange ids;                 // handle these ids; assume nothing about the others
	const SimHost::EventBatch& events;    // new since this registration's last run (empty without an event mask)
};

} // namespace KT1

#endif
