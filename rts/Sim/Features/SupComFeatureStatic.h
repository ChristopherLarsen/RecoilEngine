/* SupCom: skip the per-frame previous-transform save for features that did not move (render-only, 2026-11-19).
 * CFeatureHandler::UpdatePreFrame saved every feature every sim frame (72 % of sim time with 32k static props).
 * Skipping is exact: if the synced matrix, position and every piece transform equal what was saved, the save would
 * write the same values. Nothing synced reads preFrameTra or prevModelSpaceTra (interpolation only). */
#ifndef SUPCOM_FEATURE_STATIC_H
#define SUPCOM_FEATURE_STATIC_H

#include <cstring>
#include "Feature.h"

inline bool SupComFeatureUnchanged(CFeature* f)
{
	const CMatrix44f& m = f->GetTransformMatrixRef(true);
	static_assert(sizeof(m) == sizeof(f->supcomSavedMat), "CMatrix44f is 16 floats");
	if (std::memcmp(&m, f->supcomSavedMat, sizeof(m)) != 0) {
		std::memcpy(f->supcomSavedMat, &m, sizeof(m));
		return false;
	}
	if (std::memcmp(&f->pos, &f->preFrameTra.t, sizeof(float3)) != 0)
		return false;
	for (const auto& lmp: f->localModel.pieces) {
		if (!lmp.SupComPrevSame())
			return false;
	}
	return true;
}

#endif
