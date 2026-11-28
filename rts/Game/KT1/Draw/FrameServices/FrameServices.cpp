/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: stubs of FrameState.h and FrameBridge.h (LUA-FREE H01). FRAME step V01 writes the bodies.

#include "FrameBridge.h"
#include "FrameState.h"

namespace KT1::FrameServices {

const FrameState& State() {
	static const FrameState none;
	return none;
}

uint32_t Changes(Core::View::FrameWhat) { return 0; }

} // namespace KT1::FrameServices

namespace KT1 {

void PushFrameBridge(lua_State*) {}

} // namespace KT1
