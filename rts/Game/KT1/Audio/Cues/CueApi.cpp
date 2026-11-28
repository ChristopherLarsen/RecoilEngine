/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: stubs of CueApi.h and CueBridge.h (LUA-FREE H01). FRAME step D10 writes the bodies.

#include "CueApi.h"
#include "CueBridge.h"
#include "Game/KT1/Core/Cues/CueEvents.h"
#include "Game/KT1/Core/Cues/CueState.h"

namespace KT1::Cues {

bool Raise(std::string_view, int, std::string_view) { return false; }
bool RaiseAt(std::string_view, const float3&, int, std::string_view) { return false; }
void Alert(std::string_view, int) {}
void AlertAt(std::string_view, std::string_view, float, float, int) {}
void ShieldHit(int, const float3&) {}
void ShieldState(int, int, std::string_view, float) {}
void CaptureProgress(int, int, float) {}
void CaptureDone(int, int) {}

} // namespace KT1::Cues

namespace KT1 {

void PushCuesBridge(lua_State*) {}

} // namespace KT1
