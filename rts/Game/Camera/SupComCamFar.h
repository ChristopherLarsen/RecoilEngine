/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* SupCom (CAM-001 R5): the player camera's far plane reaches at least this far (elmos), so the KardashevCamera's furthest view is never clipped
 * by the stock map-extent estimate (Test Flat: 5232 against a 5790 dMax). Set by CKardashevCamera::Refit to dMax x FAR_FACTOR; 0 = stock. Draw only. */
#ifndef SUPCOM_CAM_FAR_H
#define SUPCOM_CAM_FAR_H

namespace SupComCamFar {
	inline constexpr float FAR_FACTOR = 1.3f;
	inline float minFar = 0.0f;
}

#endif
