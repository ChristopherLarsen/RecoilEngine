/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the phase schedule. See Phases.h.

#include "Phases.h"

#include <array>
#include <string>

#include "Check.h"
#include "DisplayEvents.h"
#include "KT1Trace.h"
#include "Fakes.h"
#include "SimEvents.h"
#include "SyncState.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/SimHost/Ordered.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {
namespace {

// Core/Common/Rate has no default: the sim host sets the one rate, before any system can convert seconds.
const bool rateSet = (Core::Rate::SetStepsPerSecond(GAME_SPEED), true);

struct Registration {
	Phases::Fn fn;
	int reader = -1;
	unsigned timer = 0;
	bool check = false;
};

std::array<Core::SimHost::Ordered<Registration>, Core::SimHost::PHASES> schedule;

} // namespace

void Phases::Add(const char* system, Core::SimHost::Phase phase, int order, Core::SimHost::EventMask events, Fn fn) {
	const std::string timer = std::string("KT1::") + system;
	CTimeProfiler::RegisterTimer(timer.c_str());
	Registration r;
	r.fn = std::move(fn);
	r.reader = (events != 0) ? SimEvents::Subscribe(events, Fakes::Sees(system)) : -1;
	r.timer = hashString(timer.c_str());
	r.check = Check::InCheck(system);
	schedule[static_cast<size_t>(phase)].Add(order, system, std::move(r));
}

void Phases::Run(Core::SimHost::Phase phase, int frame) {
	for (const auto& item : schedule[static_cast<size_t>(phase)]) {
		const Registration& r = item.value;
		const Core::SimHost::EventBatch batch = (r.reader >= 0) ? SimEvents::Take(r.reader) : Core::SimHost::EventBatch();
		const SimContext context{frame, phase, Core::SimHost::IdRange::All(), batch, r.check};
		const Check::Running running(item.name.c_str(), r.check);
		ScopedTimer t(r.timer);
		r.fn(context);
	}
	if (phase != Core::SimHost::Phase::FrameEnd) return;
	Check::CloseStep(frame);
	Trace::CloseStep(frame);
	SyncState::Close();
	SimEvents::Compact();
	DisplayEvents::Publish(frame);
}

bool SimContext::Order(int unitID, const ::Command& command) const {
	return Write([&]() {
		CUnit* unit = unitHandler.GetUnit(unitID);
		if (unit != nullptr) unit->commandAI->GiveCommand(command);
	});
}

bool SimContext::Emit(const Core::SimHost::DisplayEvent& e) const { return DisplayEvents::Emit(e); }

void Phases::Reset() {
	for (auto& p : schedule) p.Clear();
}

} // namespace KT1
