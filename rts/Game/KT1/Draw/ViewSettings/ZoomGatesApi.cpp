/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: ZoomGatesApi.h before FRAME step D07: the facts' level, every gate open.

#include "ZoomGatesApi.h"

#include "Game/KT1/Draw/Common/FrameFacts.h"

namespace KT1::ZoomGates {

int Level(const FrameFacts& facts) { return facts.ZoomLevel(); }
bool WholeMap() { return false; }
GateId Gate(std::string_view) { return NO_GATE; }
bool GateOpen(GateId) { return true; }
float GateValue(GateId, float other) { return other; }
uint32_t Changes() { return 0; }

} // namespace KT1::ZoomGates
