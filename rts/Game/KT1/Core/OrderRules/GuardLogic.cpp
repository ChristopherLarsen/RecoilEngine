/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core: stubs behind GuardLogic.h (LUA-FREE H01; steps S11 and S14 port them).

#include "GuardLogic.h"

namespace KT1::Core::OrderRules {

void State::Init(int32_t) {}
DenseIndex::Slot State::Track(UnitId) { return DenseIndex::NONE; }
void State::Forget(UnitId) {}
void State::BeginStep() {}
bool State::Reissuing(UnitId, int32_t) const { return false; }
void State::SetReissuing(UnitId, int32_t, bool) {}

bool BeyondMap(float, float, float, float) { return false; }
bool EnemyUnseen(bool, bool, int32_t) { return false; }
bool Unreclaimable(UnitId, UnitId, const Orders::DefFacts&) { return false; }
Orders::CommandKind AttackMoveRewrite(const Orders::DefFacts&) { return Orders::CommandKind::Other; }
bool PackAccepts(const Orders::DefFacts&, Orders::CommandKind, bool) { return false; }
bool DefAccepts(const Orders::DefFacts&, Orders::CommandKind) { return false; }
Orders::FireState NextFireState(Orders::FireState current, bool, float) { return current; }
Orders::FireState FireFromEngine(int32_t, int32_t) { return Orders::FireState::Hold; }
bool DiveWanted(bool, float, bool) { return false; }
bool ShortMove(float, float, float) { return false; }
bool NearQueuedMove(std::span<const Orders::OrderView>, float, float, float) { return false; }
void RestPoint(float, float, float, float, float, float, float&, float&) {}
bool PlanUnloadPoints(float, float, float, float, float, float, float, UnloadPoints&) { return false; }
bool HoldsSpot(float, float, float, float, float, float) { return false; }
bool KnownNotSeen(int32_t) { return false; }
bool KnownTargetInRange(float, float) { return false; }
void SortAreaFeatures(std::span<AreaFeature>) {}
bool Still(float, float, float) { return false; }
bool AskReclaim(ReclaimStill&, bool, Step, Step) { return false; }
int32_t TransferKeep(bool, int32_t) { return 0; }

} // namespace KT1::Core::OrderRules
