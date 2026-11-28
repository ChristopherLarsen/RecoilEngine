/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the change counters of the frame services (LUA-FREE H01, FRAME §4.1 and §7: Core tests cover them). No
// engine include. One counter per thing the remaining Lua and the native readers poll; a reader acts when its counter
// differs from the one it saw last.

#ifndef KT1_CORE_FRAME_CHANGES_H
#define KT1_CORE_FRAME_CHANGES_H

#include <cstdint>
#include <string_view>

namespace KT1::Core::View {

enum class FrameWhat : uint8_t { Selection, Hover, Camera, Zoom, Mouse, Count };

constexpr int FRAME_WHATS = static_cast<int>(FrameWhat::Count);

// "selection", "hover", "camera", "zoom", "mouse"; Count for any other name
FrameWhat ParseFrameWhat(std::string_view name);

class FrameChanges {
public:
	// once per drawn frame and thing; the first note after Reset counts as a change
	void Note(FrameWhat what, bool differs);
	uint32_t Count(FrameWhat what) const;   // 0 before the first note
	void Reset();

private:
	uint32_t counts[FRAME_WHATS] = {};
};

} // namespace KT1::Core::View

#endif
