/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComAirRest.h.

#include "SupComAirRest.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "Game/GameSetup.h"
#include "Map/ReadMap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/Rectangle.h"

namespace {
	constexpr int CELL = 32;          // heightmap squares per stamp cell side (DESIGN: coarse; a change wakes a 256-elmo region)
	constexpr int MARGIN = 2;         // squares: the smooth normal reads the neighbouring squares, the height its own

	struct Snap {
		const UnitDef* def = nullptr;
		int creationFrame = -1;
		unsigned stamp = 0;           // the change counter when recorded
		float pos[3];
		float speed[4];
		float front[3], right[3], up[3];
		short heading = 0;
		bool valid = false;
	};

	std::vector<Snap> snaps;
	std::vector<unsigned> cells;      // the change counter value of the last change touching each cell
	int cellsX = 0, cellsZ = 0;
	unsigned counter = 0;
	int enabled = -1;

	bool Enabled() {
		if (enabled < 0) {
			const auto& mo = CGameSetup::GetModOptions();
			const auto it = mo.find("supcom_air_rest");
			enabled = (it != mo.end() && it->second == "0") ? 0 : 1;
		}
		return enabled == 1;
	}

	void EnsureCells() {
		if (cellsX > 0)
			return;
		cellsX = mapDims.mapx / CELL + 1;
		cellsZ = mapDims.mapy / CELL + 1;
		cells.assign(cellsX * cellsZ, 0u);
	}

	unsigned CellAt(const float3& p) {
		EnsureCells();
		const int sx = std::clamp(int(p.x / SQUARE_SIZE) / CELL, 0, cellsX - 1);
		const int sz = std::clamp(int(p.z / SQUARE_SIZE) / CELL, 0, cellsZ - 1);
		return cells[sz * cellsX + sx];
	}

	template<typename T> bool Same(const T* a, const void* b, size_t n) { return std::memcmp(a, b, n * sizeof(T)) == 0; }
}

void SupComAirRest::TerrainChanged(const SRectangle& r)
{
	// the whole-map update is the map's first load (CReadMap::Initialize): a new game in this process starts clean
	if (r.x1 == 0 && r.z1 == 0 && r.x2 == mapDims.mapx && r.z2 == mapDims.mapy) {
		snaps.clear();
		cells.clear();
		cellsX = cellsZ = 0;
		counter = 0;
		enabled = -1;
	}
	EnsureCells();
	++counter;
	const int x1 = std::clamp((r.x1 - MARGIN) / CELL, 0, cellsX - 1), x2 = std::clamp((r.x2 + MARGIN) / CELL, 0, cellsX - 1);
	const int z1 = std::clamp((r.z1 - MARGIN) / CELL, 0, cellsZ - 1), z2 = std::clamp((r.z2 + MARGIN) / CELL, 0, cellsZ - 1);
	for (int z = z1; z <= z2; ++z)
		for (int x = x1; x <= x2; ++x)
			cells[z * cellsX + x] = counter;
}

bool SupComAirRest::Skip(const CUnit* u)
{
	if (!Enabled() || u->id < 0 || u->id >= (int)snaps.size())
		return false;
	const Snap& s = snaps[u->id];
	if (!s.valid || s.def != u->unitDef || s.creationFrame != u->creationFrame || s.heading != static_cast<short>(u->heading))
		return false;
	if (!Same(s.pos, &u->pos.x, 3) || !Same(s.speed, &u->speed.x, 4))
		return false;
	if (!Same(s.front, &u->frontdir.x, 3) || !Same(s.right, &u->rightdir.x, 3) || !Same(s.up, &u->updir.x, 3))
		return false;
	return CellAt(u->pos) <= s.stamp;
}

void SupComAirRest::Record(const CUnit* u)
{
	if (!Enabled() || u->id < 0)
		return;
	if (u->id >= (int)snaps.size())
		snaps.resize(u->id + 1);
	Snap& s = snaps[u->id];
	// only a settled result is a fixed point: the step left the speed exactly zero (see the header)
	const float4& v = u->speed;
	s.valid = (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f && v.w == 0.0f);
	if (!s.valid)
		return;
	s.def = u->unitDef;
	s.creationFrame = u->creationFrame;
	s.stamp = counter;
	s.heading = static_cast<short>(u->heading);
	std::memcpy(s.pos, &u->pos.x, sizeof(s.pos));
	std::memcpy(s.speed, &u->speed.x, sizeof(s.speed));
	std::memcpy(s.front, &u->frontdir.x, sizeof(s.front));
	std::memcpy(s.right, &u->rightdir.x, sizeof(s.right));
	std::memcpy(s.up, &u->updir.x, sizeof(s.up));
}
