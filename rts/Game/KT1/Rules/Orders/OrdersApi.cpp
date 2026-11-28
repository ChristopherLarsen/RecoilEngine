/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: stubs behind OrdersApi.h (LUA-FREE H01; steps S05, S10 and S12 port them).

#include "OrdersApi.h"

namespace KT1::Orders {

bool IsAMove(const SimContext&, int) { return false; }
int AMoveTarget(const SimContext&, int) { return -1; }
void AmTargetOf(const SimContext&, int, std::vector<int>&) {}

Core::Orders::Task GuardTask(const SimContext&, int) { return {}; }
int GuardRepairOf(const SimContext&, int) { return -1; }
int GuardShieldOf(const SimContext&, int) { return -1; }
void GuardShields(const SimContext&, std::vector<std::pair<int, int>>&) {}
void ClearGuardShield(const SimContext&, int) {}
bool Refused(const SimContext&, int) { return false; }

Core::Orders::FireState DefaultFireState(const SimContext&, int) { return Core::Orders::FireState::Hold; }
Core::Orders::FireState FireStateOf(const SimContext&, int) { return Core::Orders::FireState::Free; }
void SetFireState(const SimContext&, int, Core::Orders::FireState) {}

bool EngineEndsBlocked(const SimContext&, int) { return false; }
bool EngineDropped(const SimContext&, float, float, float, bool, float, float, float, float) { return false; }
bool RedirectUnreachable(const SimContext&, const Core::Orders::RedirectQuery&, bool) { return false; }
bool StaleFill(const SimContext&, float, float, bool, float, float, float, float, bool) { return false; }
bool RefillDue(const SimContext&, const Core::Orders::MoveTrack&, float, float, bool) { return false; }
bool OrderFillBeyond(const SimContext&, float, float, float, float) { return false; }
bool TieNear(const SimContext&, float, float, float, float, float) { return false; }
void MarkBlockChange(const SimContext&, float, float, float) {}

bool HasFerry(const SimContext&, int) { return false; }
int FerryMarker(const SimContext&, int) { return -1; }
int FerryEndMarker(const SimContext&, int) { return -1; }
void FerryWaiting(const SimContext&, int, std::vector<int>&) {}

void NoteGoalCell(const SimContext&, int, const Core::Orders::GoalCellRecord&) {}
std::span<const Core::Orders::GoalCellRecord> GoalCellRecords(const SimContext&, int) { return {}; }

} // namespace KT1::Orders
