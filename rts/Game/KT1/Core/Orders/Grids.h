/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the two map grids the move tracking reads: where something that blocks appeared or went (so a stalled
// move knows a new reachability fill may answer differently), and where the immobile units are (the footprint probes).
// Cells are floor(x / cell) by floor(z / cell); positions off the map fall into the margin. No engine include.

#ifndef KT1_CORE_ORDERS_GRIDS_H
#define KT1_CORE_ORDERS_GRIDS_H

#include <cstdint>
#include <vector>

#include "OrderTypes.h"

namespace KT1::Core::Orders {

class ChangeGrid {
public:
	// map size and the margin kept round it, elmos; allocates once
	void Init(float mapX, float mapZ, float cellElmo, float marginElmo);
	// a blocking change round (x, z) at this step: every cell the square of half side `radius` touches
	void Mark(float x, float z, float radius, Step step);
	// a cell within `reach` of (x, z) changed at `since` or later
	bool Near(float x, float z, float reach, Step since) const;

private:
	std::vector<Step> cells;
	float cell = 0;
	int32_t first = 0;      // the margin, in cells
	int32_t wide = 0;
	int32_t deep = 0;
};

// Immobile units by the cell of their base position. A query's results are only tested for yes or no, so its order does
// not reach the sim.
class StructureGrid {
public:
	void Init(float mapX, float mapZ, float cellElmo, int32_t maxUnits);
	void Add(UnitId unit, float x, float z);
	void Remove(UnitId unit);
	// appends the units of every cell the square of half side `reach` round (x, z) touches; the caller tests each one's
	// current mid point
	void Candidates(float x, float z, float reach, std::vector<UnitId>& out) const;

private:
	std::vector<std::vector<UnitId>> cells;
	std::vector<int32_t> cellOf;   // by unit id; -1: not in the grid
	float cell = 0;
	int32_t wide = 0;
	int32_t deep = 0;
};

} // namespace KT1::Core::Orders

#endif
