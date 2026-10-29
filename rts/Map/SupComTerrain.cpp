/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComTerrain.h"

#include "MapDamage.h"
#include "ReadMap.h"
#include "System/Config/ConfigHandler.h"

CONFIG(bool, TerrainFollowHeights).defaultValue(true).description("supcom: the derived terrain (normals, slopes) follows the heights in every frame a crater or terraform changes them, and the recalculation covers building footprints and a terraform's last step (1); stock updates only at the end of a crater or on the builder's slow update (0).");

namespace SupComTerrain {
	bool FollowHeights()
	{
		static const bool on = configHandler->GetBool("TerrainFollowHeights");
		return on;
	}

	void UpdateDerived(const Area& a)
	{
		readMap->UpdateHeightMapSynced({std::max(a.x1, 0), std::max(a.z1, 0), std::min(a.x2, mapDims.mapx), std::min(a.z2, mapDims.mapy)});
	}

	void Recalc(const Area& a, int r)
	{
		mapDamage->RecalcArea(a.x1 - r, a.x2 + r, a.z1 - r, a.z2 + r);
	}
}
