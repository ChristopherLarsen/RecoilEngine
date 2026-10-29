/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_TERRAIN_H
#define SUPCOM_TERRAIN_H

#include <algorithm>

// supcom: the derived terrain (centre heights, normals, slopes) follows the heights in every frame they change, during
// craters and terraforming, and the full recalculation (LOS, paths, smooth mesh) covers every square that changed:
// building footprints lowered with a crater, and a terraform's last step. Stock updates the derived terrain only at the
// end (a crater) or on the builder's SlowUpdate (a terraform), so a game loaded in between, which rebuilds the derived
// terrain from the heights, differed from the saved one. Springsetting TerrainFollowHeights (default 1; 0 is stock).
namespace SupComTerrain {
	struct Area { int x1, x2, z1, z2; };

	bool FollowHeights();

	/// readMap->UpdateHeightMapSynced on the area, clamped to the map
	void UpdateDerived(const Area& a);
	/// mapDamage->RecalcArea on the area grown by r squares
	void Recalc(const Area& a, int r);

	/// an explosion step's squares and the footprints of its buildings still standing (isAlive(id)), one square around
	template<typename Buildings, typename IsAlive>
	Area ExplosionArea(int x1, int x2, int y1, int y2, const Buildings& buildings, IsAlive isAlive) {
		Area a = {x1 - 1, x2 + 1, y1 - 1, y2 + 1};
		for (const auto& b: buildings) {
			if (!isAlive(b.id))
				continue;
			a.x1 = std::min(a.x1, b.tx1 - 1); a.x2 = std::max(a.x2, b.tx2 + 1);
			a.z1 = std::min(a.z1, b.tz1 - 1); a.z2 = std::max(a.z2, b.tz2 + 1);
		}
		return a;
	}
}

#endif
