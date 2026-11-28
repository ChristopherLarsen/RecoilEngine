/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind GuardRules.h (LUA-FREE H01; steps S05 and S12 port them).

#include "GuardRules.h"

namespace KT1::Core::Orders {

void Nearest::OfferByLowestId(UnitId, float) {}
void Nearest::OfferFirst(UnitId, float) {}
bool NeedsRepair(float, float, float, float) { return false; }
GuardParams MakeGuardParams(const OrdersConfig&) { return {}; }
FollowPlan PlanFollow(const FollowFacts&, const GuardParams&) { return {}; }
AssistWant WantedAssist(const Task&, UnitId, UnitId, const DefFacts&, int32_t) { return {}; }
bool WantsShield(float, float, bool, bool) { return false; }

ReachParams MakeReachParams(const OrdersConfig&) { return {}; }
bool ReachCheckDue(UnitId, Step, bool, const ReachParams&) { return false; }
bool NoteReach(ReachStall&, CommandKind, float, float, float, Step, const ReachParams&) { return false; }
float SpotRadius(float, float, const ReachParams&) { return 0; }
void SpotPoint(int32_t, float, float, float, const ReachParams&, float&, float&) {}
bool SpotTried(const ReachStall&, float, float, const ReachParams&) { return false; }
void NoteSpotSearch(ReachStall&, float, Step) {}
void NoteSpot(ReachStall&, float, float) {}
Walk PlanWalk(float, float, float, float, float, float, float, float, const ReachParams&) { return {}; }
void BeginWalk(ReachStall&, float, float, Step, const ReachParams&) {}
bool WalkArrived(float, float, float, float, float, const ReachParams&) { return false; }
bool WalkExpired(const ReachStall&, Step) { return false; }
void EndWalk(ReachStall&, Step) {}

} // namespace KT1::Core::Orders
