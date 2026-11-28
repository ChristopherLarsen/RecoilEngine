/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: calls synced systems at fixed points of the sim step, all inside CGame::SimFrame:
//   FrameStart   immediately before the Lua GameFrame call-in (native first, then Lua: fixed), after the engine rules
//   AfterUnits   after unitHandler.Update(): movement and weapons both done
//   FrameEnd     after projectiles, everything else and Lua GameFramePost; then the step is closed and published
// A phase between unit movement and weapon update does not exist: it would need a stock hook inside the unit update,
// agreed with the New Engine plan.
//
// A system registers once per phase it works in:
//   KT1::Phases::Add("Census", Phase::FrameStart, 0, MaskOf(EventType::UnitCreated), [this](const SimContext& c) {...});
// Register at game setup (Rule::LoadConfig: before the first unit exists).
// Within a phase the order is (order, system name). Each registration has its own event reader, so it sees every event
// of its mask exactly once. Each run is timed as "KT1::<system>". A phase function handles context.ids only; a system
// with a DueWheel takes the step's bucket once and hands ranges of it to its slices (never one Take per slice).

#ifndef KT1_HOST_PHASES_H
#define KT1_HOST_PHASES_H

#include <functional>

#include "SimContext.h"

namespace KT1 {

class Phases {
public:
	using Fn = std::function<void(const SimContext&)>;

	static void Add(const char* system, Core::SimHost::Phase phase, int order, Core::SimHost::EventMask events, Fn fn);
	// KT1 hook: SimHost (CGame::SimFrame, one call per phase). FrameEnd also closes the step: check counts, sync
	// checksum, event lists, display events.
	static void Run(Core::SimHost::Phase phase, int frame);
	static void Reset();   // game shutdown
};

} // namespace KT1

#endif
