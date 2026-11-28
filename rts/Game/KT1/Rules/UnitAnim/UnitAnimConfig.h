/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UnitAnim: its config.
// Engine-free. Values: game-build game/data/kt1/ (full labels there). The members hold the defaults, each
// with its source label.

#ifndef KT1_UNITANIM_CONFIG_H
#define KT1_UNITANIM_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct UnitAnimConfig {
	int idleTicks = 2;          // DESIGN: still animation ticks before an idle unit backs off (a count: tick length varies)
	float restDelayS = 3.0f;    // DESIGN: seconds with no aim call before a mount turns to rest
	float restRate = 1.5f;      // DESIGN: rad/s back to the rest pose
	float rollEps = 0.05f;      // DESIGN: degrees: the body turn is re-issued past this change
	int wakeEvery = 3;          // PLACEHOLDER: animation ticks between dust events of a moving unit (a count)
	float spinRate = 0.6f;      // DESIGN: cycles per second turning on the spot
	int fullUnits = 250;        // DESIGN: units alive up to which the tick is 0.1 s
	float maxTickS = 0.5f;      // DESIGN: seconds, the longest tick

	void Describe(Data::Schema& s) {
		s.Field("idle_ticks", idleTicks, Data::AtLeast(1));
		s.Field("rest_delay_s", restDelayS, Data::AtLeast(0));
		s.Field("rest_rate", restRate);
		s.Field("roll_eps", rollEps);
		s.Field("wake_every", wakeEvery, Data::AtLeast(1));
		s.Field("spin_rate", spinRate);
		s.Field("full_units", fullUnits, Data::AtLeast(1));
		s.Field("max_tick_s", maxTickS, Data::AtLeast(0));
	}
};

} // namespace KT1

#endif
