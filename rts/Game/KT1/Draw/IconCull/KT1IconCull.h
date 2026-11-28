/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 IconCull: the engine's icon loops skip units whose icon draws nothing (a fully transparent bitmap), before any projection.
// Included only by rts/Rendering/Units/UnitDrawer.cpp (its "// KT1 hook: IconCull" lines). See KT1IconCull.cpp.

#ifndef KT1_ICON_CULL_H
#define KT1_ICON_CULL_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace KT1 {
namespace IconCull {
	extern bool active;                 // the rule is on (mod option kt1_iconcull)
	extern std::vector<uint8_t> empty;  // per icon index: 1 = its bitmap is transparent everywhere; 2 = not yet checked
	extern size_t skipped;              // units skipped this drawn frame (stats)

	bool Check(size_t iconIdx);         // reads the icon's bitmap once; true when it is empty

	// One line in each loop: true when this unit's icon would draw nothing.
	inline bool Skip(size_t iconIdx) {
		if (!active) return false;
		const uint8_t v = iconIdx < empty.size() ? empty[iconIdx] : 2;
		const bool e = (v == 2) ? Check(iconIdx) : (v == 1);
		skipped += e ? 1 : 0;
		return e;
	}
}
}

#endif
