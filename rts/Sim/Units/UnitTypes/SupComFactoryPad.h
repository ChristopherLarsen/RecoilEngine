/* supcom (ECO-012 / FAC: factory pad-free delay): a factory unit def with customParams.supcom_pad_clear (elmos) starts its next
   build once every mobile unit standing on the build square has its centre at least that far from the build position (the pack's
   clear distance); structures and features block as before. Without the key: the stock test (any other object on the build
   square blocks). Unit defs are synced, so the rule is the same on every client. */
#pragma once

#include "System/float3.h"

class CFactory;

namespace SupComFactoryPad {
	bool Blocked(const CFactory* factory, const float3& buildPos);
	// FAC-002 v6 R11 (pathing fix 2026-11-25): the turn of the unit on the build point, in heading units, from the factory's unit rules
	// param "supcom_build_spin" (degrees, set by the game when it chooses the exit point; synced); 0 without it
	int BuildSpin(const CFactory* factory);
}
