/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 IconCull: its config.
// Engine-free. Values: game-build game/data/kt1/ (labels there); the members hold the defaults.

#ifndef KT1_ICONCULL_CONFIG_H
#define KT1_ICONCULL_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct IconCullConfig {
	int alphaMax = 0;   // an icon is empty when no pixel's alpha is above this

	void Describe(Data::Schema& s) { s.Field("alpha_max", alphaMax, Data::Between(0, 255)); }
};

} // namespace KT1

#endif
