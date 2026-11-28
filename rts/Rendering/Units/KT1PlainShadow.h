/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */

/* KT1 plain shadows (2026-11-27 shadow root-cause): unit types whose shadow the game draws with a Lua shadow material that is a plain
 * depth write (Kardashev shaders/art_shadow: no alpha discard, no slope bias, polygon offset 1,1). Flagged types skip the Lua material
 * in the shadow pass and are drawn in the instanced GL4 pass in that same plain form, instead of one legacy draw per unit and piece.
 * Set by Spring.KT1.Draw.SetPlainShadow from unsynced Lua. Draw only. */
#ifndef KT1_PLAIN_SHADOW_H
#define KT1_PLAIN_SHADOW_H

#include <cstdint>
#include <vector>

namespace KT1PlainShadow {
	inline std::vector<uint8_t> defs;
	// the unit draw mask is tested before the Lua material bins (opaque, alpha, shadow), as for features; off: stock order
	// (Spring.KT1.Draw.SetUnitMaskBeforeMaterial; default off, reset at game start)
	inline bool maskFirst = false;
	inline bool Is(int defID) { return defID >= 0 && defID < static_cast<int>(defs.size()) && defs[defID] != 0; }
	inline void Set(int defID, bool v) {
		if (defID < 0) return;
		if (static_cast<size_t>(defID) >= defs.size()) defs.resize(defID + 1, 0);
		defs[defID] = v ? 1 : 0;
	}
}

#endif
