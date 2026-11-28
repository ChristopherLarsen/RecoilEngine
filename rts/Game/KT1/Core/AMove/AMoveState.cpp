/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove core: stubs behind AMoveState.h (LUA-FREE H01; step S03 ports them).

#include "AMoveState.h"

namespace KT1::Core::AMove {

int32_t ModeParam(Mode) { return 0; }
const char* EventName(EventKind) { return ""; }
Params MakeParams(const Orders::OrdersConfig&, const Orders::AMoveConfig&, std::span<const Orders::DefFacts>) { return {}; }
float GuardRadius(const Orders::DefFacts&, const Params&) { return 0; }
StripShape ShapeFor(float, const Params&) { return {}; }

void State::Init(int32_t, int32_t, const Params&) {}
DenseIndex::Slot State::Start(UnitId, uint32_t, const Goal&, Step) { return DenseIndex::NONE; }
void State::End(UnitId) {}
bool State::Running(UnitId, uint32_t) const { return false; }
void State::SetMode(UnitId, Mode, UnitId) {}
std::span<const Attacker> State::AttackersOf(UnitId) const { return {}; }
void State::TargetDied(UnitId) {}
void State::MarkFrontChanged(UnitId) {}
void State::Schedule(UnitId) {}
void State::TakeDue(Step, std::span<const UnitId>, std::vector<UnitId>&) {}
void State::NoteStarted(UnitId, float, float) {}
void State::BeginStep() {}

Step NextUpkeep(uint8_t&, UnitId, Step, Step) { return Orders::NO_STEP; }
Step NextPick(uint8_t&, UnitId, Step, Step) { return Orders::NO_STEP; }
FightPlan PlanFight(bool, bool, float, float) { return FightPlan::Drop; }
float CloseRadius(float, const Params&) { return 0; }
bool Arrived(Headway&, Mode, float, bool, Step, const Params&) { return false; }
bool StillReturning(Headway&, float, Step, const Params&) { return false; }
bool Beats(const PickKey&, const PickKey&) { return false; }
uint64_t GroupKey(float, float) { return 0; }

} // namespace KT1::Core::AMove
