/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the phase schedule. See Phases.h.

#include "Phases.h"

#include <array>
#include <string>

#include "DisplayEvents.h"
#include "SimEvents.h"
#include "SyncState.h"
#include "Game/KT1/Core/SimHost/Ordered.h"
#include "Sim/Misc/GlobalConstants.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {
namespace {

struct Registration {
	Phases::Fn fn;
	int reader = -1;
	unsigned timer = 0;
};

std::array<SimHost::Ordered<Registration>, SimHost::PHASES> schedule;

} // namespace

void Phases::Add(const char* system, SimHost::Phase phase, int order, SimHost::EventMask events, Fn fn) {
	const std::string timer = std::string("KT1::") + system;
	CTimeProfiler::RegisterTimer(timer.c_str());
	Registration r;
	r.fn = std::move(fn);
	r.reader = (events != 0) ? SimEvents::Subscribe(events) : -1;
	r.timer = hashString(timer.c_str());
	schedule[static_cast<size_t>(phase)].Add(order, system, std::move(r));
}

void Phases::Run(SimHost::Phase phase, int frame) {
	for (const auto& item : schedule[static_cast<size_t>(phase)]) {
		const Registration& r = item.value;
		const SimHost::EventBatch batch = (r.reader >= 0) ? SimEvents::Take(r.reader) : SimHost::EventBatch();
		const SimContext context{frame, GAME_SPEED, phase, SimHost::IdRange::All(), batch};
		ScopedTimer t(r.timer);
		r.fn(context);
	}
	if (phase != SimHost::Phase::FrameEnd) return;
	SyncState::Close();
	SimEvents::Compact();
	DisplayEvents::Publish();
}

void Phases::Reset() {
	for (auto& p : schedule) p.Clear();
}

} // namespace KT1
