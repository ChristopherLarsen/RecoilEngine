/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComGapSnap.h"

#include <algorithm>
#include <cmath>

#include "Map/ReadMap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/MoveMath/MoveMath.h"
#include "System/Config/ConfigHandler.h"

CONFIG(bool, SupComGapSnap).defaultValue(true).description("supcom: path waypoints in a gap exactly a footprint wide go onto the gap's centre line (PTH-004-E)");

namespace {
	bool On() { static const bool on = configHandler->GetBool("SupComGapSnap"); return on; }

	bool Blocked(const MoveDef& md, int sx, int sz) {
		if (sx < 0 || sz < 0 || sx > mapDims.mapxm1 || sz > mapDims.mapym1)
			return true;
		return (CMoveMath::GetPosSpeedMod(md, sx, sz) <= 0.0f);
	}

	// across axis (0 = x, 1 = z) at square (sx, sz): the centre (elmos) of an open run exactly fs squares wide with blocked
	// squares on both ends, or a negative value
	float GapCentre(const MoveDef& md, int sx, int sz, int axis, int fs) {
		if (Blocked(md, sx, sz))
			return -1.0f;
		const int lim = fs + 2;
		int lo = 0, hi = 0;
		bool loF = false, hiF = false;
		for (int k = 1; k <= lim && !loF; k++) {
			if (Blocked(md, sx - (axis == 0) * k, sz - (axis == 1) * k)) { lo = k; loF = true; }
		}
		for (int k = 1; k <= lim && !hiF; k++) {
			if (Blocked(md, sx + (axis == 0) * k, sz + (axis == 1) * k)) { hi = k; hiF = true; }
		}
		if (!loF || !hiF || (lo + hi - 1) != fs)
			return -1.0f;
		const int c = (axis == 0)? sx: sz;
		// open squares c - lo + 1 .. c + hi - 1: centre of [ (c - lo + 1) * S, (c + hi) * S ]
		return (float((c - lo + 1) + (c + hi)) * SQUARE_SIZE * 0.5f);
	}
}

void SupComGapSnap::Snap(IPath::Path& path, const MoveDef& md)
{
	if (!On() || path.path.size() < 3)
		return;

	const int fs = std::max(1, int(md.xsize));
	auto& pts = path.path;
	const int n = int(pts.size());

	// the first and last points are the goal and the start: kept
	for (int i = 1; i < n - 1; i++) {
		const int sx = int(pts[i].x / SQUARE_SIZE);
		const int sz = int(pts[i].z / SQUARE_SIZE);

		for (int axis = 0; axis < 2; axis++) {
			const float c = GapCentre(md, sx, sz, axis, fs);
			if (c < 0.0f)
				continue;

			// this point and up to two neighbours on the same line (within a square of the centre)
			for (int j = std::max(1, i - 2); j <= std::min(n - 2, i + 2); j++) {
				float& v = (axis == 0)? pts[j].x: pts[j].z;
				if (std::fabs(v - c) <= SQUARE_SIZE)
					v = c;
			}
			break;
		}
	}
}
