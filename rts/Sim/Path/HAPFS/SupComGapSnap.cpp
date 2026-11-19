/* supcom, see SupComGapSnap.h */

#include "SupComGapSnap.h"

#include "Map/ReadMap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GroundBlockingObjectMap.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/MoveMath/MoveMath.h"
#include "Sim/Objects/SolidObject.h"

#include <cmath>

static bool Wall(const MoveDef& md, int x, int z)
{
	// impassable square for the move def: off the map, terrain it cannot cross, or a structure on it
	if (x < 0 || z < 0 || x >= mapDims.mapx || z >= mapDims.mapy)
		return true;
	if (CMoveMath::GetPosSpeedMod(md, unsigned(x), unsigned(z)) <= 0.0f)
		return true;

	const CGroundBlockingObjectMap::BlockingMapCell& cell = groundBlockingObjectMap.GetCellUnsafeConst(z * mapDims.mapx + x);
	for (size_t i = 0, n = cell.size(); i < n; i++) {
		if (cell[i]->immobile)
			return true;
	}
	return false;
}

// a gap exactly f squares wide across axis k (0: across x, 1: across z) at square (x, z): its centre in elmos, or -1
static float GapCentre(const MoveDef& md, int x, int z, int k, int f)
{
	if (Wall(md, x, z))
		return -1.0f;

	const int ax = (k == 0)? 1: 0;
	const int az = 1 - ax;
	int lo = -1, hi = -1;

	for (int d = 1; d <= f + 1; d++) {
		if (Wall(md, x - ax * d, z - az * d)) { lo = d; break; }
	}
	for (int d = 1; d <= f + 1; d++) {
		if (Wall(md, x + ax * d, z + az * d)) { hi = d; break; }
	}
	if (lo < 0 || hi < 0 || (lo + hi - 1) != f)
		return -1.0f;

	const int c = (k == 0)? x: z;
	return float((c - lo + 1) + (c + hi)) * (SQUARE_SIZE * 0.5f);
}

void SupComGapSnap::Snap(IPath::path_list_type& path, const MoveDef& md, bool keepFront)
{
	const int f = md.xsize;
	if (f <= 0 || f != md.zsize || path.size() < 2)
		return;

	const size_t first = keepFront? 1: 0;
	const size_t last = path.size() - 1;     // the start, not snapped

	for (size_t i = first; i < last; i++) {
		float3& w = path[i];
		const int x = int(w.x / SQUARE_SIZE);
		const int z = int(w.z / SQUARE_SIZE);

		for (int k = 0; k < 2; k++) {
			// the gap at the waypoint or up to 2 squares along the passage (k = 0: the passage runs along z)
			static constexpr int ALONG[5] = {0, -1, 1, -2, 2};
			float c = -1.0f;
			for (int a: ALONG) {
				c = (k == 0)? GapCentre(md, x, z + a, 0, f): GapCentre(md, x + a, z, 1, f);
				if (c >= 0.0f)
					break;
			}
			if (c < 0.0f)
				continue;

			float& across = (k == 0)? w.x: w.z;
			if (std::abs(across - c) <= SQUARE_SIZE * 1.5f)
				across = c;
			break;
		}
	}
}
