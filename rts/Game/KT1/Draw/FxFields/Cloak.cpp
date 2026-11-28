/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxFields: the cloak rules. Stub (LUA-FREE H01): step D14 writes the bodies.

#include "Game/KT1/Draw/FxFields/Cloak.h"

namespace KT1::FxFields {

void InitCloaks(CloakState&, const CloakConfig&, int32_t) {}
void TrackCloak(CloakState&, int32_t, bool, float, const CloakConfig&) {}
void ForgetCloak(CloakState&, int32_t) {}
CloakChange SetCloaked(Cloak&, float, bool) { return CloakChange::None; }
Relation RelationOf(int32_t, int32_t, int32_t, int32_t, bool) { return Relation::Enemy; }
CloakDrawing DrawingOf(const CloakConfig&, const Cloak&, float, Relation, bool) { return {}; }

} // namespace KT1::FxFields
