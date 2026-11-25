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
	// MOV-021 v1 R4: a unit is in the pushed state while last frame's push exceeds this fraction of its top speed (mod option
	// supcompushclear; default the pack value G0-PATHFOL contact.push_clear_speed_fraction 0.1)
	float PushClearFrac();
	// MOV-021 v1 R1: the push back on a blocked step, as a fraction of top speed (mod option supcompushback; PLACEHOLDER 0.25, the size
	// is open in MOV-021)
	float PushBackFrac();
	// MOV-021 v1 R1: no slide along an obstacle (mod option supcomnoslide, default 1): the engine's own movement no longer takes over
	// a unit refused by a structure
	bool NoSlide();
}

#endif
