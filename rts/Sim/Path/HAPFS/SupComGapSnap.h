/* supcom: PTH-004-E. Waypoints in (or up to 2 squares before or after) a gap exactly one footprint wide are moved onto
   the gap centre line across the gap. Deterministic: integer square maths, the result is a multiple of SQUARE_SIZE / 2. */
#pragma once

#include "IPath.h"

struct MoveDef;

namespace SupComGapSnap {
	// path.back() is the start and is left alone; keepFront leaves path.front() (the goal) alone too
	void Snap(IPath::path_list_type& path, const MoveDef& md, bool keepFront);
}
