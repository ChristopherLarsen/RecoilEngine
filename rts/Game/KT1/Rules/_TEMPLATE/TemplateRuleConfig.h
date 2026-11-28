/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: a synced system's config. Engine-free. Copy to Rules/<System>/<System>Config.h.
// Every tunable is a member holding its default and source label, named in Describe by its key in the game archive's
// data/kt1/<system>.json. Times are seconds, never frames. A synced system reads its file once per game.

#ifndef KT1_TEMPLATE_RULE_CONFIG_H
#define KT1_TEMPLATE_RULE_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct TemplateRuleConfig {
	float periodS = 0.5f;   // DESIGN: seconds between two visits of one unit
	float gain = 1.0f;      // PLACEHOLDER: charge added per visit
	float cap = 10.0f;      // PLACEHOLDER: the largest charge

	void Describe(Data::Schema& s) {
		s.Field("period_s", periodS, Data::AtLeast(0));
		s.Field("gain", gain);
		s.Field("cap", cap, Data::AtLeast(0));
	}
};

} // namespace KT1

#endif
