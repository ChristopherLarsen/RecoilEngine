/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxDecals: the API of the decal system. Stub (LUA-FREE H01): step D16 writes the bodies.

#include "Game/KT1/Draw/Decals/DecalsApi.h"

#include "Game/KT1/Draw/Decals/DecalRequest.h"

namespace KT1::FxDecals {

bool Active() { return false; }
uint32_t Submit(const DecalRequest&) { return 0; }
bool ScorchDeath(float, float, float, float, float) { return false; }

void PushFxDecalsBridge(lua_State*) {}

} // namespace KT1::FxDecals
