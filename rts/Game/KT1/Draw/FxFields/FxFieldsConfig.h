/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: the system's config (game/data/kt1/fxfields.json). The large blast's timeline is a file of its own
// (NukeBlast.h, fxnuke.json). Engine-free.

#ifndef KT1_FX_FIELDS_CONFIG_H
#define KT1_FX_FIELDS_CONFIG_H

#include "Game/KT1/Draw/FxFields/Blasts.h"
#include "Game/KT1/Draw/FxFields/Cloak.h"
#include "Game/KT1/Draw/FxFields/Depth.h"
#include "Game/KT1/Draw/FxFields/Shields.h"
#include "Game/KT1/Draw/FxFields/Strategic.h"

namespace KT1::FxFields {

struct FxFieldsConfig {
	ShieldConfig shields;
	CloakConfig cloak;
	BlastConfig blasts;
	DepthConfig depth;
	StrategicLookConfig strategic;
	template<class S> void Describe(S& s) {
		s.Object("shields", [&](auto& o) { shields.Describe(o); });
		s.Object("cloak", [&](auto& o) { cloak.Describe(o); });
		s.Object("blasts", [&](auto& o) { blasts.Describe(o); });
		s.Object("depth", [&](auto& o) { depth.Describe(o); });
		s.Object("strategic", [&](auto& o) { strategic.Describe(o); });
	}
};

} // namespace KT1::FxFields

#endif
