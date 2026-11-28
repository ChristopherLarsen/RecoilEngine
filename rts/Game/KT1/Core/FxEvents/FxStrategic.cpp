/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the missile relay. Stub (LUA-FREE H01): step S30 writes the body.

#include "Game/KT1/Core/FxEvents/FxStrategic.h"

#include "Game/KT1/Core/FxEvents/FxState.h"

namespace KT1::Core::Fx {

void RelayStrategic(StrategicState&, const StrategicConfig&, std::span<const MissileIn>, std::span<const InterceptorIn>,
	std::span<const RedirectIn>, int32_t, const WorldQuery&, FxOutput&) {}

} // namespace KT1::Core::Fx
