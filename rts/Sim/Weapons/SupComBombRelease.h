/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_BOMB_RELEASE_H
#define SUPCOM_BOMB_RELEASE_H

#include "System/float3.h"

struct WeaponDef;
class CUnit;

/**
 * SupCom bomb release (MOV-016 v5 R5), selected per weapon with the weapon-definition custom parameter
 * `supcom_bomb_impact = "1"` (absent or "0": the stock release).
 *
 * Stock CBombDropper releases as soon as the predicted impact point (the carrier's position plus its velocity times
 * the fall time) comes within half a salvo's length of the target, so a single bomb leaves that far early. With the
 * switch the bomb is released when the predicted impact point reaches the target: the target is at most zero and at
 * least two frames of flight ahead of the point along the carrier's horizontal velocity, and across that line within
 * the bomb's own lateral correction (CBombDropper::FireImpl adds at most one elmo per frame of fall), or the stock
 * window if that is wider. The predicted impact point is measured from the muzzle, where the bomb leaves (stock measures
 * it from the aim point but the fall time from the muzzle); the fall time is the stock one (myGravity, the carrier's
 * vertical speed).
 */
namespace SupComBombRelease {
	bool Enabled(const WeaponDef* def);
	bool InRange(const CUnit* owner, const float3& dropPos, const float3& tgtPos, float fallTime, float stockWindow);
}

#endif
