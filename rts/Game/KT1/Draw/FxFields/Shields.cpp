/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxFields: the shield rules. Stub (LUA-FREE H01): step D14 writes the bodies.

#include "Game/KT1/Draw/FxFields/Shields.h"

namespace KT1::FxFields {

void InitShields(ShieldState&, const ShieldConfig&, int32_t) {}
ShieldChange OnShieldState(ShieldState&, const ShieldConfig&, const Core::Fx::ShieldState&, float) {
	return ShieldChange::None;
}
int32_t OnShieldHit(ShieldState&, const ShieldConfig&, int32_t, const Vec3&, const Vec3&, float) { return -1; }
const ShieldStyle& StyleOf(const ShieldConfig& c, Core::Fx::Faction) { return c.human; }
float ShieldOpacity(const ShieldConfig&, float) { return 0.0f; }
float ShieldFlicker(const ShieldConfig&, float) { return 0.0f; }
Rgb ShieldTint(const ShieldConfig&, Core::Fx::Faction, float) { return {}; }
Rgb ShieldRim(const ShieldConfig&, const Rgb&, const Rgb&) { return {}; }
float RippleAge(const ShieldConfig&, const Shield&, int32_t, float) { return -1.0f; }
float Assemble(const ShieldConfig&, const Shield&, float) { return 0.0f; }
float Down(const ShieldConfig&, const Shield&, float) { return -1.0f; }
bool Animating(const ShieldConfig&, const Shield&, float) { return false; }
bool ShieldVisible(const ShieldConfig&, const Shield&, float, bool) { return false; }
float DrawRadius(const ShieldConfig&, const Shield&, float) { return 0.0f; }
float DomeCode(const Shield&, float) { return 0.0f; }
ShieldInstance PackShield(const ShieldConfig&, const Shield&, const ShieldPlace&, float, bool) { return {}; }

} // namespace KT1::FxFields
