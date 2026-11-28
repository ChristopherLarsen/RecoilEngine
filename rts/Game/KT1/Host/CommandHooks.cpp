/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: see CommandHooks.h.

#include "CommandHooks.h"

#include <utility>

#include "Check.h"
#include "Fakes.h"
#include "Game/KT1/Core/SimHost/Validators.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"

namespace KT1 {
namespace {

Core::SimHost::Chain<CommandHooks::FallbackFn> fallbacks;

} // namespace

void CommandHooks::AddFallback(const char* system, int order, FallbackFn fn) {
	fallbacks.Add(order, system, {system, Check::InCheck(system), std::move(fn), Fakes::Sees(system)});
}

void CommandHooks::Reset() { fallbacks = {}; }

bool CommandHooks::AskFallback(const CUnit* u, const Command& c, bool& remove) {
	if (fallbacks.Empty()) return false;
	for (const auto& item : fallbacks) {
		const Check::Running running(item.value.system.c_str(), item.value.check);
		const Fallback answer = item.value.fn(u, c, Faked{Fakes::Is(u)});
		if (item.value.check) {
			Check::Value v;
			v.v[0] = (answer != Fallback::Pass) ? 1.0f : 0.0f;
			v.v[1] = (answer == Fallback::Done) ? 1.0f : 0.0f;
			v.count = Check::Value::BOTH;
			Check::Native(item.value.system.c_str(), "CommandFallback", u->id, c.GetID(), 0, v);
		} else if (answer != Fallback::Pass) {
			remove = (answer == Fallback::Done);
			return true;
		}
	}
	return false;
}

} // namespace KT1
