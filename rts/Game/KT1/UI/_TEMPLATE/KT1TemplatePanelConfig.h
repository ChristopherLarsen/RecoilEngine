/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: an interface panel's tunables, with defaults and source labels. Copy to UI/<System>/<System>Config.h.
// Filled from gamedata/kt1/<system>.lua (JSON with live reload once the F04 data layer is in).

#ifndef KT1_TEMPLATE_PANEL_CONFIG_H
#define KT1_TEMPLATE_PANEL_CONFIG_H

namespace KT1 {

struct TemplatePanelConfig {
	int max_rows = 64;   // DESIGN: unit defs the panel lists
};

} // namespace KT1

#endif
