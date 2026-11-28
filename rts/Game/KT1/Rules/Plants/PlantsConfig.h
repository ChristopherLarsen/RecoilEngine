/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Plants: its config.
// Engine-free. Values: game-build game/data/kt1/ (full labels there). The members hold the defaults, each
// with its source label.

#ifndef KT1_PLANTS_CONFIG_H
#define KT1_PLANTS_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct PlantsConfig {
	float treesEveryS = 0.1f;           // DESIGN: seconds between the tree checks of one unit
	float treesReach = 0.6f;            // DESIGN: share of the footprint half-width at which a tree is reached
	float treesMinSpeedPerS = 0.29999998f;   // DESIGN: elmos per second above which a unit knocks trees over
	float plantsEveryS = 0.1f;          // DESIGN: seconds between the plant contact checks of one unit
	float plantsMargin = 4.0f;          // PLACEHOLDER: elmos added to the footprint half-width for contact
	float plantsMinSpeedPerS = 1.5f;    // DESIGN: elmos per second above which a unit counts as moving

	void Describe(Data::Schema& s) {
		s.Field("trees_every_s", treesEveryS, Data::AtLeast(0));
		s.Field("trees_reach", treesReach);
		s.Field("trees_min_speed_per_s", treesMinSpeedPerS);
		s.Field("plants_every_s", plantsEveryS, Data::AtLeast(0));
		s.Field("plants_margin", plantsMargin);
		s.Field("plants_min_speed_per_s", plantsMinSpeedPerS);
	}
};

} // namespace KT1

#endif
