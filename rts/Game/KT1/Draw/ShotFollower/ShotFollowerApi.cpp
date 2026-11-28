/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ShotFollower: the API of the shot follower. Stub (LUA-FREE H01): step D05 writes the bodies.

#include "Game/KT1/Draw/ShotFollower/ShotFollowerApi.h"

#include "Game/KT1/Draw/ShotFollower/ShotTrack.h"

namespace KT1::ShotFollower {

bool Active() { return false; }
std::span<const Glow> Glows() { return {}; }
bool Reload(const char*) { return false; }

void PushShotFollowerBridge(lua_State*) {}

} // namespace KT1::ShotFollower
