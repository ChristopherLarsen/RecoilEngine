/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Census: its config.
// Engine-free. Values: game-build game/data/kt1/ (full labels there). The members hold the defaults, each
// with its source label.

#ifndef KT1_CENSUS_CONFIG_H
#define KT1_CENSUS_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct CensusConfig {
	float periodS = 1.0f;           // DESIGN: seconds between tallies
	int defaultWeight = 1;          // DESIGN: a unit's weight when its def sets none
	bool countUnfinished = false;   // DESIGN: count units still being built

	void Describe(Data::Schema& s) {
		s.Field("period_s", periodS, Data::AtLeast(0));
		s.Field("default_weight", defaultWeight);
		s.Field("count_unfinished", countUnfinished);
	}
};

} // namespace KT1

#endif
