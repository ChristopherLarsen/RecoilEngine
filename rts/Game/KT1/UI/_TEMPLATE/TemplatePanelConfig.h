/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: an interface panel's config. Engine-free. Copy to UI/<System>/<System>Config.h.
// Every tunable is a member holding its default and source label, named in Describe by its key in the game archive's
// data/kt1/<system>.json. An unsynced system's file may be read again while the game runs (Rule::ConfigReloaded).

#ifndef KT1_TEMPLATE_PANEL_CONFIG_H
#define KT1_TEMPLATE_PANEL_CONFIG_H

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct TemplatePanelConfig {
	int maxRows = 64;   // DESIGN: unit defs the panel lists

	void Describe(Data::Schema& s) { s.Field("max_rows", maxRows, Data::AtLeast(0)); }
};

} // namespace KT1

#endif
