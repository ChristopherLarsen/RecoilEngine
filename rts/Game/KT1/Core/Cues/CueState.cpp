/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of CueState.h (LUA-FREE H01). FRAME step D10 writes the bodies.

#include "CueState.h"

namespace KT1::Core::Cues {

CueParams MakeParams(const CueConfig&) { return {}; }

void CueState::Init(int32_t, int32_t, const CueConfig&) {}
void CueState::AddUnit(int32_t, const CueUnitKind&) {}
bool CueState::RemoveUnit(int32_t) { return false; }

int32_t CueId(CueState&, std::string_view) { return -1; }
void BeginStep(CueState&, const CueParams&, int32_t) {}
bool Admit(CueState&) { return false; }
void Log(CueState&, int32_t, int32_t, const float (&)[3]) {}
bool RowMatches(const CueRow&, std::string_view, std::string_view, std::string_view, TeamRelation) { return false; }
bool RowAdmit(CueState&, const CueConfig&, int32_t) { return false; }
bool BeamAdmit(CueState&, const CueParams&, int32_t) { return false; }
bool MoveDue(const CueParams&, int32_t, int32_t) { return false; }
bool MoveChanged(CueState&, int32_t, bool) { return false; }
bool LoopChanged(CueState&, int32_t, bool) { return false; }
uint8_t FuelStep(CueState&, const CueParams&, int32_t, const FuelFacts&) { return 0; }
void FuelForget(CueState&, int32_t) {}
CaptureCue CaptureStep(CueState&, const CueParams&, int32_t, int32_t, float) { return CaptureCue::None; }
void CaptureEnd(CueState&, int32_t) {}
LayerCue LayerStep(CueState&, int32_t, bool) { return LayerCue::None; }
ShieldCue ShieldStateStep(CueState&, int32_t, int, bool) { return ShieldCue::None; }
bool ShieldHitAdmit(CueState&, const CueParams&, int32_t) { return false; }
bool ZoomHeld(float, float) { return false; }

} // namespace KT1::Core::Cues
