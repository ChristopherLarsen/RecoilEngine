/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: the family's calls into target choice (family COMBAT), in one place. Until COMBAT is native the bodies call
// the Lua chooser (GG.Cmb.Engage, Engage2, BestKey, focus, ScanFrames) through the bridge; afterwards COMBAT's
// Rules/Targeting/TargetingApi.h. The attack-move's own order of choice is Core/AMove (PickKey, Beats). Unit ids; -1 is
// no unit. Ranges in elmos.

#ifndef KT1_RULES_ORDERS_TARGET_PICK_H
#define KT1_RULES_ORDERS_TARGET_PICK_H

#include <span>

namespace KT1 {

struct SimContext;

namespace Orders {

struct AMovePickQuery {
	std::span<const int> strip;      // the seen enemies of the strip around the remaining route, ascending
	float x = 0, z = 0;              // the unit's place
	bool air = false;                // an aircraft never halts for a harmless enemy
	float vision = 0;                // a harmless enemy is waited for within this
};
struct AMovePick {
	int target = -1;                 // the best armed enemy the unit can take
	bool primary = false;            // weapon 1 takes it
	int harmless = -1;               // only when there is no target: the nearest unarmed enemy in sight it could hit
};
// ord_orders.lua Pick, over the strip
AMovePick PickTarget(const SimContext& context, int unit, const AMovePickQuery& query);

// The reach and rank at which the unit's weapons can take the target; can false: none, or the unit holds fire.
struct EngageAnswer {
	bool can = false;
	float reach = 0;
	int rank = 0;
};
// ignoreLine: do not test the line of fire. onlyWeapon: 0 any weapon, else that weapon number alone (1 is primary).
EngageAnswer Engage(const SimContext& context, int unit, int target, bool ignoreLine, int onlyWeapon);
// the attack-move's focus: every weapon that can take it and reaches it fires at it; target -1 clears
void SetFocus(const SimContext& context, int unit, int target);
// the steps between target checks of the def's first scanned weapon (SIM-RATE conversion is COMBAT's); 0: none
int ScanSteps(const SimContext& context, int unitDefID);

} // namespace Orders
} // namespace KT1

#endif
