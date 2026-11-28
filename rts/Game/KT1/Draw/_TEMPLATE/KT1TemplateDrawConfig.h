/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: a draw system's tunables, with defaults and source labels. Copy to Draw/<System>/<System>Config.h.
// Filled from gamedata/kt1/<system>.lua (JSON with live reload once the F04 data layer is in).

#ifndef KT1_TEMPLATE_DRAW_CONFIG_H
#define KT1_TEMPLATE_DRAW_CONFIG_H

#include <array>
#include <cstdint>

namespace KT1 {

struct TemplateDrawConfig {
	float half_size = 8.0f;        // DESIGN: half the dot's side, elmos
	float full_charge = 10.0f;     // PLACEHOLDER: the charge drawn fully opaque
	std::array<uint8_t, 4> colour = {60, 140, 255, 200};   // DESIGN: red, green, blue, full opacity
};

} // namespace KT1

#endif
