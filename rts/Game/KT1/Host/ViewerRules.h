/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: what the local viewer may know of a unit or feature: LuaUI's own rules (LuaUtils::IsAllyUnit and friends;
// CLuaUI::UpdateTeams: a full view reads everything, else the local ally team). For Host/ only: it includes Sim/.

#ifndef KT1_HOST_VIEWERRULES_H
#define KT1_HOST_VIEWERRULES_H

#include "Game/GlobalUnsynced.h"
#include "Sim/Features/Feature.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

namespace KT1::ViewerRules {

inline bool ReadsAsAlly(const CUnit& u) { return gu->spectatingFullView || u.allyteam == gu->myAllyTeam; }

// IsUnitVisible: the unit exists for the viewer
inline bool Seen(const CUnit& u) {
	return ReadsAsAlly(u) || (u.losStatus[gu->myAllyTeam] & (LOS_INLOS | LOS_INRADAR)) != 0;
}

// IsUnitInLos
inline bool InSight(const CUnit& u) { return ReadsAsAlly(u) || (u.losStatus[gu->myAllyTeam] & LOS_INLOS) != 0; }

// IsUnitTyped: in sight, or seen before and not lost from radar since
inline bool Typed(const CUnit& u) {
	if (ReadsAsAlly(u)) return true;
	const unsigned short los = u.losStatus[gu->myAllyTeam];
	constexpr unsigned short prev = LOS_PREVLOS | LOS_CONTRADAR;
	return (los & LOS_INLOS) != 0 || (los & prev) == prev;
}

// GetUnitDefID: the def id the viewer may know, 0 when the type is unknown
inline int DefId(const CUnit& u) {
	if (ReadsAsAlly(u)) return u.unitDef->id;
	if (!Typed(u)) return 0;
	return (u.unitDef->decoyDef != nullptr) ? u.unitDef->decoyDef->id : u.unitDef->id;
}

// IsFeatureVisible
inline bool Seen(const CFeature& f) { return gu->spectatingFullView || f.IsInLosForAllyTeam(gu->myAllyTeam); }

} // namespace KT1::ViewerRules

#endif
