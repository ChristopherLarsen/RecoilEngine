/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: def tables read at Start, and the engine step fed with them (LUA-FREE S19).

#ifndef KT1_TARGETING_DEFS_H
#define KT1_TARGETING_DEFS_H

#include "Game/KT1/Core/Targeting/TargetState.h"
#include "Lua/SupComTargeting.h"

namespace KT1::Targeting {

// cmb_targeting.lua's WD, MF, classesOf and ranks; unit_cmd_gun.lua's rank
void ReadDefs(Core::Targeting::TargetingDefs& defs, const Core::Targeting::TargetingConfig& cfg);
// cmb_targeting.lua's Spring.SupComTgtInit(1, inset, skip, overkill, defs, ranks): the engine step chooses
void FeedEngineStep(const Core::Targeting::TargetingDefs& defs, const Core::Targeting::TargetingParams& params,
	const Core::Targeting::TargetingConfig& cfg);

} // namespace KT1::Targeting

#endif
