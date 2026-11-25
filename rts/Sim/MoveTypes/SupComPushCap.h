/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* SupCom pathing fix (game docs/engine/PATHING-ROOT-CAUSE.md cause 3 and 4): a SupComGroundMoveType unit is moved by its own step
 * and, in the same frame, by the engine collision push. The push is capped so that the step and the push together stay within a
 * fraction of the unit top speed (no visible jump), with a floor so an overlap still resolves. Included only by SupComGroundMoveType.cpp. */
#ifndef SUPCOM_PUSH_CAP_H
#define SUPCOM_PUSH_CAP_H

#include "System/float3.h"

namespace SupComPushCap {
	// push: this frame collision push (applied after the step); step: this frame own step; maxSpeed: top speed, elmos per frame
	void Clamp(float3& push, const float3& step, float maxSpeed);
	// mod option supcompushcap (default 1): 0 leaves the push as the engine computes it
	bool On();
}

#endif
