/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_GAP_SNAP_H
#define SUPCOM_GAP_SNAP_H

#include "IPath.h"

struct MoveDef;

// supcom (PTH-004-E, docs/engine/PTH-004-E-GAP-SNAP-DESIGN.md in the game): a max-resolution waypoint in a gap exactly as wide
// as the move def's footprint is moved onto the gap's centre line across it (and up to two neighbours on the same line), so a
// unit held at the gap does not steer into the wall squares either side. Integer square arithmetic on the move def's speed
// mods: as deterministic as the path. Off with springsetting SupComGapSnap=0.
namespace SupComGapSnap {
	void Snap(IPath::Path& path, const MoveDef& md);
}

#endif
