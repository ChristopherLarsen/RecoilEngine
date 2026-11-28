/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: calls synced systems at fixed points of the sim step (Core/SimHost/Phase.h says where each phase is).
//
// A system registers at game setup (Rule::LoadConfig: before the first unit exists), once per phase it works in:
//   KT1::Phases::Add("Census", Phase::FrameStart, 0, MaskOf(EventType::UnitCreated), [this](const SimContext& c) {...});
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
