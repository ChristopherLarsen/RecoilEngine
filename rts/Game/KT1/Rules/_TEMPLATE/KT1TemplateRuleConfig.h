/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: a synced system's tunables, with defaults and source labels. Copy to Rules/<System>/<System>Config.h.
// Every tunable is a field here, filled from gamedata/kt1/<system>.lua (JSON once the F04 data layer is in). Times are
// seconds, never frames. A changed value needs a new game, never a rebuild.

#ifndef KT1_TEMPLATE_RULE_CONFIG_H
#define KT1_TEMPLATE_RULE_CONFIG_H

namespace KT1 {

struct TemplateRuleConfig {
	float period_s = 0.5f;   // DESIGN: seconds between two visits of one unit
	float gain = 1.0f;       // PLACEHOLDER: charge added per visit
	float cap = 10.0f;       // PLACEHOLDER: the largest charge
};

} // namespace KT1

#endif
