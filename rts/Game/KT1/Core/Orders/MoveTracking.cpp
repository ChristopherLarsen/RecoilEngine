/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind MoveTracking.h (LUA-FREE H01; step S10 ports them).

#include "MoveTracking.h"

namespace KT1::Core::Orders {

TrackParams MakeTrackParams(const OrdersConfig&) { return {}; }
bool TrackingDue(Step, bool, const TrackParams&) { return false; }

MoveTrack BeginTrack(uint32_t, float, float, Step, float, float) { return {}; }
MoveTrack RedirectedTrack(uint32_t, float, float, float, float, Step, Step, int32_t) { return {}; }
void NoteGain(MoveTrack&, float, Step, const TrackParams&) {}
void NoteSpeed(MoveTrack&, float, Step, const TrackParams&) {}
void ForcedStop(MoveTrack&, float, float, Step) {}
bool WaitedByPosition(MoveTrack&, float, float, Step, const TrackParams&) { return false; }
bool IsStuck(const MoveTrack&, float, Step, const TrackParams&) { return false; }
bool EndsAtRedirectStop(const MoveTrack&, float, Step, const TrackParams&) { return false; }
bool ShortOfGoal(float, float, float, float, const TrackParams&) { return false; }
Step StaleDue(const MoveTrack&, const TrackParams&) { return NO_STEP; }
Step GiveUpDue(const MoveTrack&, const TrackParams&) { return NO_STEP; }

bool AheadPoint(float, float, float, float, const TrackParams&, float&, float&) { return false; }
bool CountsAsProgressing(float, const MoveTrack*, Step, const TrackParams&) { return false; }

float BeyondElmo(const OrdersConfig&) { return 0; }
bool OrderFillBeyond(float, float, float, float, float) { return false; }
Step NextRefill(Step, UnitId, Step) { return NO_STEP; }
bool FillWanted(const MoveTrack&, Step, bool, const TrackParams&) { return false; }
void RecordSkippedFill(MoveTrack&, UnitId, Step, float, float, const TrackParams&) {}
bool RefillDue(const MoveTrack&, Step, float, float, const ChangeGrid&, bool, const TrackParams&) { return false; }
void PostponeRefill(MoveTrack&, UnitId, Step, const TrackParams&) {}
bool MakeUpFillWanted(const MoveTrack&, Step, bool, const TrackParams&) { return false; }
bool BudgetLeft(const State&, const TrackParams&) { return false; }
void NoteFill(MoveTrack&, UnitId, Step, float, float, Reach, const TrackParams&) {}

Redirect RedirectUnreachable(const RedirectQuery&, const TrackParams&) { return Redirect::No; }
bool StaleIfNotBoxed(float, float, bool, float, float, float, float, float) { return false; }
bool TieNear(float, float, float, float, float, float) { return false; }
bool EngineDropped(float, float, float, bool, float, float, float, float, const TrackParams&) { return false; }
void TiePoint(int32_t, float, float, float, const TrackParams&, float&, float&) {}
void RedirectAim(float, float, float, float, bool, const TrackParams&, float&, float&) {}
bool RetryDroppedRedirect(const MoveTrack&, bool, bool, Step, const TrackParams&) { return false; }

void BoxProbe(int32_t, int32_t, float, float, float, float&, float&) {}
bool FootprintsOverlap(float, float, float, float, float, float, float, float) { return false; }
bool InYard(float, float, float, float, float, float) { return false; }
bool InFootprint(float, float, float, float, float, float) { return false; }

} // namespace KT1::Core::Orders
