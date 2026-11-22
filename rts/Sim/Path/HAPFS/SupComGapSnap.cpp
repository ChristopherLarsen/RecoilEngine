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
			// the nearest one-footprint gap whose centre square is up to 4 squares (2 cells) along the passage and up to 4 across from the
			// waypoint (k = 0: a passage along z, centre line across x; k = 1: along x, across z); the waypoint is moved onto the gap's
			// centre line when it lies within 4 squares of it across the passage (an approach waypoint a cell off the line, L9250)
			static constexpr int REACH = 4;
			float c = -1.0f;
			int bestD = 1 << 20;
			for (int along = -REACH; along <= REACH; along++) {
				for (int cross = -REACH; cross <= REACH; cross++) {
					const int d = std::abs(along) * 2 + std::abs(cross);
					if (d >= bestD)
						continue;
					const float g = (k == 0)? GapCentre(md, x + cross, z + along, 0, f): GapCentre(md, x + along, z + cross, 1, f);
					if (g >= 0.0f) {
						c = g;
						bestD = d;
					}
				}
			}
			if (c < 0.0f)
				continue;

			float& across = (k == 0)? w.x: w.z;
			if (std::abs(across - c) <= SQUARE_SIZE * REACH)
				across = c;
			break;
		}
	}
}
