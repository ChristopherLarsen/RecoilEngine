/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind QueueRules.h (LUA-FREE H01; step S05 ports them).

#include "QueueRules.h"

namespace KT1::Core::Orders {

bool IsEngineWork(CommandKind) { return false; }
bool EngineAhead(std::span<const OrderView>, int32_t) { return false; }
bool CountsInternal(std::span<const OrderView>, int32_t) { return false; }
int32_t OrderIndex(std::span<const OrderView>) { return -1; }
TurnPath PathOf(const OrderView&, const DefFacts&) { return TurnPath::ReadQueue; }
bool StartsAMove(const OrderView&, bool, uint32_t) { return false; }
bool RemovesEngineWork(std::span<const OrderView>, int32_t, const OwnStep&, bool, bool) { return false; }
bool WasGiven(std::span<const GivenWork>, CommandKind, float) { return false; }
AssistFound FindAssistStep(std::span<const OrderView>, const ReachWalk&, std::span<const GivenWork>, const OwnStep&) {
	return {};
}
int32_t LeadWorkIndex(std::span<const OrderView>, const ReachWalk&, std::span<const GivenWork>, const OwnStep&) {
	return -1;
}
void RecordGiven(std::vector<GivenWork>&, CommandKind, std::span<const float>, bool) {}
Task WorkOf(const OrderView&, const WorkFacts&, int32_t) { return {}; }
Step NextPatrolCheck(Step, Step, Step, UnitId) { return NO_STEP; }
FrontClass ClassifyFront(CommandKind, TurnPath, bool, bool) { return FrontClass::None; }

} // namespace KT1::Core::Orders
