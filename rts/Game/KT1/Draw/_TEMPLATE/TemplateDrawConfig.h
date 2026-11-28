/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: a draw system's config. Engine-free. Copy to Draw/<System>/<System>Config.h.
// Every tunable is a member holding its default and source label, named in Describe by its key in the game archive's
// data/kt1/<system>.json. An unsynced system's file may be read again while the game runs (Rule::ConfigReloaded).

#ifndef KT1_TEMPLATE_DRAW_CONFIG_H
#define KT1_TEMPLATE_DRAW_CONFIG_H

#include <array>

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct TemplateDrawConfig {
	float halfSize = 8.0f;      // DESIGN: half the dot's side, elmos
	float fullCharge = 10.0f;   // PLACEHOLDER: the charge drawn fully opaque
	std::array<float, 4> colour = {60, 140, 255, 200};   // DESIGN: red, green, blue, full opacity; 0 to 255

	void Describe(Data::Schema& s) {
		s.Field("half_size", halfSize, Data::AtLeast(0));
		s.Field("full_charge", fullCharge, Data::AtLeast(0));
		s.Field("colour", colour, Data::Between(0, 255));
	}
};

} // namespace KT1

#endif
