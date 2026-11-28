/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Tiers: its config.
// Engine-free. Values: game-build game/data/kt1/ (labels there); the members hold the defaults.

#ifndef KT1_TIERS_CONFIG_H
#define KT1_TIERS_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct TiersConfig {
	float farPeriodS = 0.5f;      // seconds between the updates of an idle distant unit
	float marginElmos = 80.0f;    // elmos added to a unit's reach
	float cellElmos = 512.0f;     // occupancy grid cell

	void Describe(Data::Schema& s) {
		s.Field("far_period_s", farPeriodS, Data::AtLeast(0));
		s.Field("margin_elmos", marginElmos);
		s.Field("cell_elmos", cellElmos, Data::AtLeast(64));
	}
};

} // namespace KT1

#endif
