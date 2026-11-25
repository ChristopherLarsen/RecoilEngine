/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule Tiers: the hook query. Included only by the files with a "// KT1 hook: Tiers" line. See KT1Tiers.cpp.

#ifndef KT1_TIERS_H
#define KT1_TIERS_H

#include <cstdint>
#include <vector>

class CUnit;

namespace KT1 {
namespace Tiers {
	extern bool on;                       // kt1_tiers, set at the rule's Start
	extern std::vector<uint8_t> asleep;   // by unit id, for this frame (set at the end of the previous frame)
	bool LiveWake(const CUnit* u);        // promotion at once: order, damage, transport, nano, stun, target, motion

	/// true: skip this unit's tiered work this frame (move step, CAI and weapon slow updates, weapon update and vectors)
	bool Asleep(const CUnit* u);
}
}

#endif
