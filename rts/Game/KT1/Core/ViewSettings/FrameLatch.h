/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: when a pointer ray must be cast again (LUA-FREE V01). No engine include. The frame services cast a ray the
// first time a reader asks, and again only when what it depends on moved: so a widget called between two frames still gets
// the answer Spring.TraceScreenRay would give it, and a frame nobody reads costs no ray.

#ifndef KT1_CORE_FRAME_LATCH_H
#define KT1_CORE_FRAME_LATCH_H

#include <array>
#include <cstdint>

namespace KT1::Core::View {

// everything a ray through a screen pixel depends on
struct RayKey {
	uint32_t drawFrame = 0;
	int step = 0;                     // the sim step: units move with it
	int x = 0, y = 0;                 // the pixel, y from the bottom
	std::array<int, 4> view{};        // view origin and size
	std::array<float, 7> cam{};       // camera position, direction, field of view
	std::array<int, 4> miniMap{};     // its rectangle; all zero when minimized or absent

	bool operator==(const RayKey&) const = default;
};

template<typename Key> class FrameLatch {
public:
	// true: the key differs from the one held, or none is held; it is held afterwards
	bool Moved(const Key& next) {
		if (held && next == key) return false;
		key = next;
		held = true;
		return true;
	}
	void Reset() { held = false; }

private:
	Key key{};
	bool held = false;
};

} // namespace KT1::Core::View

#endif
