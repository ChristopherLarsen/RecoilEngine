/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// supcom: two CEG tags on every spawnable class (weapon VFX gaps 2 and 3). Drawing only; the simulation never reads them.
//   supcomAdditive  0..1: 0 keeps the class blend; 1 makes the particle add light. The effects pass blends premultiplied
//                   (ONE, ONE_MINUS_SRC_ALPHA), so scaling a vertex alpha by (1 - supcomAdditive) with the colour kept
//                   turns "over" into "add" without a new pass or a sort change.
//   supcomStretch   >= 0: the quad is lengthened along its on-screen velocity by 1 + supcomStretch x speed (elmos per
//                   frame), about its own centre; a still particle is unchanged.

#include "ExpGenSpawnable.h"

#include <algorithm>

#include "Game/Camera.h"

void CExpGenSpawnable::SupComAdjustQuad(VA_TYPE_TC* q) const
{
	if (supcomAdditive != 0.0f) {
		const float keep = 1.0f - std::clamp(supcomAdditive, 0.0f, 1.0f);
		for (int i = 0; i < 4; ++i)
			q[i].c.a = static_cast<uint8_t>(q[i].c.a * keep);
	}

	if (supcomStretch != 0.0f && speed.w > 1e-4f) {
		const float3 fwd = camera->GetForward();
		float3 v = float3(speed.x, speed.y, speed.z);
		v -= fwd * v.dot(fwd); // the part of the velocity across the screen
		const float vl = v.Length();
		if (vl > 1e-4f) {
			v /= vl;
			const float extra = std::max(0.0f, supcomStretch) * speed.w; // added length factor
			const float3 c = (q[0].pos + q[1].pos + q[2].pos + q[3].pos) * 0.25f;
			for (int i = 0; i < 4; ++i) {
				const float3 d = q[i].pos - c;
				q[i].pos += v * (d.dot(v) * extra);
			}
		}
	}
}
