/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 ShotFollower: the per-shot steps. Stub (LUA-FREE H01): step D05 writes the bodies.

#include "Game/KT1/Draw/ShotFollower/ShotTrack.h"

namespace KT1::ShotFollower {

void InitFollower(FollowerState&, const FollowerConfig&, int32_t) {}
void FollowStep(FollowerState&, const FollowIn&, std::span<const ShotIn>, std::span<const Core::Fx::WeaponFx* const>,
	const FollowWorld&) {}
void EndShot(FollowerState&, const FollowIn&, const Core::Fx::ShotEnd&, const FollowWorld&) {}
void ExpireStains(FollowerState&, float) {}

} // namespace KT1::ShotFollower
