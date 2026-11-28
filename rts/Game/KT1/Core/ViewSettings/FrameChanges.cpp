/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of FrameChanges.h (LUA-FREE H01). FRAME step V01 writes the bodies.

#include "FrameChanges.h"

namespace KT1::Core::View {

FrameWhat ParseFrameWhat(std::string_view) { return FrameWhat::Count; }
void FrameChanges::Note(FrameWhat, bool) {}
uint32_t FrameChanges::Count(FrameWhat) const { return 0; }
void FrameChanges::Reset() {}

} // namespace KT1::Core::View
