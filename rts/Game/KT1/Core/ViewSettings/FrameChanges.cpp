/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: FrameChanges.h.

#include "FrameChanges.h"

namespace KT1::Core::View {

FrameWhat ParseFrameWhat(std::string_view name) {
	constexpr std::string_view NAMES[FRAME_WHATS] = {"selection", "hover", "camera", "zoom", "mouse"};
	for (int i = 0; i < FRAME_WHATS; ++i) {
		if (NAMES[i] == name) return static_cast<FrameWhat>(i);
	}
	return FrameWhat::Count;
}

void FrameChanges::Note(FrameWhat what, bool differs) {
	if (what >= FrameWhat::Count) return;
	uint32_t& n = counts[static_cast<int>(what)];
	if (differs || n == 0) ++n;
}

uint32_t FrameChanges::Count(FrameWhat what) const {
	return what < FrameWhat::Count ? counts[static_cast<int>(what)] : 0;
}

void FrameChanges::Reset() {
	for (uint32_t& n : counts) n = 0;
}

} // namespace KT1::Core::View
