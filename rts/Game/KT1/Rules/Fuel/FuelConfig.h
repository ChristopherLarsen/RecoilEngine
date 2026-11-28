/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Fuel: its config.
// Engine-free. Values: game-build game/data/kt1/ (labels there); the members hold the defaults.

#ifndef KT1_FUEL_CONFIG_H
#define KT1_FUEL_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct FuelConfig {
	float climbAbove = 4.0f;            // elmos above the ground at which an aircraft counts as flying
	float gaugeStep = 0.005f;           // share of the capacity the gauge moves before it is sent again
	float lowEveryS = 0.16666667f;      // seconds between the low-fuel looks at one aircraft
	float lowFraction = 0.19f;          // low-fuel fraction where the unit gives none

	void Describe(Data::Schema& s) {
		s.Field("climb_above", climbAbove);
		s.Field("gauge_step", gaugeStep);
		s.Field("low_every_s", lowEveryS, Data::AtLeast(0));
		s.Field("low_fraction", lowFraction);
	}
};

} // namespace KT1

#endif
