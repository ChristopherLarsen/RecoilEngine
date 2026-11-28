/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OwnPass: its config.
// Engine-free. Values: game-build game/data/kt1/ (labels there); the members hold the defaults.

#ifndef KT1_OWNPASS_CONFIG_H
#define KT1_OWNPASS_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct OwnPassConfig {
	float sliceS = 0.1f;           // seconds between the speed reads of one unit
	float ahead = 24.0f;           // elmos beyond the two sizes at which the light unit stops blocking
	float searchMargin = 64.0f;    // elmos added to the search radius around a light unit
	float idleSpeedPerS = 1.5f;    // elmos per second below which a unit counts as standing

	void Describe(Data::Schema& s) {
		s.Field("slice_s", sliceS, Data::AtLeast(0));
		s.Field("ahead", ahead);
		s.Field("search_margin", searchMargin);
		s.Field("idle_speed_per_s", idleSpeedPerS);
	}
};

} // namespace KT1

#endif
