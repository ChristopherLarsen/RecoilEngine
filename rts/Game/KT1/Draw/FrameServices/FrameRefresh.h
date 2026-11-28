/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the parts of FrameState one at a time, for the bridge (a widget that asks for the mouse casts no ray).
// Each call brings its part up to the engine's state now, notes a change, and returns the shared state.

#ifndef KT1_DRAW_FRAME_REFRESH_H
#define KT1_DRAW_FRAME_REFRESH_H

#include <string>
#include <utility>
#include <vector>

#include "FrameState.h"

namespace KT1::FrameServices {

const FrameState& Selection();
const FrameState& Pointer();          // hover.pointer and the minimap point
const FrameState& Centre();           // hover.centreGround
const FrameState& Camera();
const FrameState& Zoom();
const FrameState& Mouse();
bool IsSelected(int id);

// the camera controller's name and every value of its state, as Spring.GetCameraState gives them (filled by Camera())
const std::string& CameraName();
const std::vector<std::pair<std::string, float>>& CameraValues();

void Reset();                         // a new game

} // namespace KT1::FrameServices

#endif
