/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the API of the build and criteria effects. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/FxBuildApi.h"

#include "Game/KT1/Draw/FxBuild/FxBuildConfig.h"

namespace KT1::FxBuild {

bool Active() { return false; }
bool CriteriaOn() { return false; }
void Hit(float, float, float, int32_t) {}

void PushFxBuildBridge(lua_State*) {}

} // namespace KT1::FxBuild
