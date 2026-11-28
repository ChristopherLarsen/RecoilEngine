/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the one native event client. It records the synced events systems ask for into Core/SimHost's batches.
// Systems do not use this directly: Phases::Add takes the event mask and hands each phase function its batch.
//
// The client is first in every event list, so it records an event before any Lua gadget sees it, and events raised from
// inside a Lua call-in follow the one that caused them. It only records; Lua call-ins run as before.

#ifndef KT1_HOST_SIM_EVENTS_H
#define KT1_HOST_SIM_EVENTS_H

#include "Game/KT1/Core/SimHost/SimEventLog.h"

namespace KT1 {

class SimEvents {
public:
	// A new reader of the types in `mask`. Recording starts here: subscribe at game setup, before the first unit exists.
	static int Subscribe(Core::SimHost::EventMask mask);
	static Core::SimHost::EventBatch Take(int reader);
	static void Compact();   // the step's end: no batch alive
	static void Reset();     // game shutdown: the client leaves the event handler, every reader is forgotten
};

} // namespace KT1

#endif
