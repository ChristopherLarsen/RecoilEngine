/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: see HostReset.h.

#include "HostReset.h"

#include "AllowChain.h"
#include "Check.h"
#include "DisplayEvents.h"
#include "Phases.h"
#include "SimEvents.h"
#include "SyncState.h"

namespace KT1 {

// In the reverse of start order: what the steps filled first, then what the systems registered at game setup.
void ResetSimHost() {
	DisplayEvents::Reset();
	Check::Reset();
	SyncState::Reset();
	AllowChain::Reset();
	SimEvents::Reset();
	Phases::Reset();
}

} // namespace KT1
