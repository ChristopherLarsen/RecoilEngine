/* SupCom: skip the per-frame previous-transform save for units that did not move (render-only, 2026-11-20).
 * CUnitHandler::UpdatePreFrame saved every active unit every sim frame (1.6 ms with 4,000 idle units). Skipping is exact:
 * if the synced matrix, position and every piece transform equal what was saved, the save would write the same values.
 * Nothing synced reads preFrameTra or prevModelSpaceTra (interpolation only). Same test as SupComFeatureStatic.h. */
#ifndef SUPCOM_UNIT_STATIC_H
#define SUPCOM_UNIT_STATIC_H

#include <cstring>
#include "Unit.h"

inline bool SupComUnitUnchanged(CUnit* u)
{
	const CMatrix44f m = u->GetTransformMatrix(true);   // what UpdatePrevFrameTransform reads
	static_assert(sizeof(m) == sizeof(u->supcomSavedMat), "CMatrix44f is 16 floats");
	if (std::memcmp(&m, u->supcomSavedMat, sizeof(m)) != 0) {
		std::memcpy(u->supcomSavedMat, &m, sizeof(m));
		return false;
	}
	if (std::memcmp(&u->pos, &u->preFrameTra.t, sizeof(float3)) != 0)
		return false;
	for (const auto& lmp: u->localModel.pieces) {
		if (!lmp.SupComPrevSame())
			return false;
	}
	return true;
}

#endif
