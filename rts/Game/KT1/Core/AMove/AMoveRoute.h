/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove core: the attack-move's route and the search for enemies in the strip around its remaining part
// (ord_orders.lua RoutePoints, SegD2, RouteD2, RouteNear, AdvanceLeg, SeenCells, StripEnemies). The route is the engine
// pathfinder's way for the unit's move class, asked once when the order starts; aircraft and failed queries: the straight
// line. No engine include.

#ifndef KT1_CORE_AMOVE_ROUTE_H
#define KT1_CORE_AMOVE_ROUTE_H

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "Game/KT1/Core/Orders/OrderTypes.h"

namespace KT1::Core::AMove {

struct Point {
	float x = 0, z = 0;
};

struct LegBox {                      // a leg's box grown by the radius
	float minX = 0, maxX = 0, minZ = 0, maxZ = 0;
};

struct Route {
	std::vector<Point> points;       // at least two once built
	int32_t leg = 0;                 // the leg the unit is on (points[leg] to points[leg + 1]); never goes back
	std::vector<LegBox> boxes;       // per leg, for boxRadius; rebuilt when the radius differs
	float boxRadius = 0;
};

// the unit's place, the path's points, and the goal when the path does not end on it; always at least one leg
void BuildRoute(Route& route, float x, float z, std::span<const Point> path, float gx, float gz);
float SegmentDistSq(float px, float pz, float ax, float az, float bx, float bz);
// squared distance from (px, pz) to the route ahead of (ax, az): to the end of the current leg, then on
float RouteDistSq(const Route& route, float px, float pz, float ax, float az);
// RouteDistSq <= g * g, skipping legs whose box does not hold the point (keeps the boxes)
bool RouteNear(Route& route, float px, float pz, float ax, float az, float g);
void AdvanceLeg(Route& route, float x, float z);      // the nearest leg at or after the current one

struct SeenUnit {
	Orders::UnitId id = Orders::NO_UNIT;
	float x = 0, z = 0;
};

// The enemies each army sees, listed by map cell: built at most once per reuse period for the armies that ask, all armies
// dropped together. A unit seen only after the build is found at the next one, as in the Lua.
class SeenCache {
public:
	void Init(int32_t allyTeams, int32_t maxUnits, float cellElmo);
	void Refresh(Orders::Step now, Orders::Step reuse);   // drops every list once `reuse` steps have passed
	bool Built(int32_t ally) const;
	void Build(int32_t ally, std::span<const SeenUnit> seen);   // ascending unit id
	std::span<const Orders::UnitId> All(int32_t ally) const;    // ascending
	// appends the units of every cell the square of half side `r` round (x, z) touches, a cell once per `visited` set
	void Cells(int32_t ally, float x, float z, float r, std::vector<int64_t>& visited,
	           std::vector<Orders::UnitId>& out) const;

private:
	struct Army {
		bool built = false;
		std::vector<Orders::UnitId> all;
		std::vector<std::pair<int64_t, Orders::UnitId>> cells;   // (cell key, unit), sorted
	};
	std::vector<Army> armies;
	float cell = 0;
	Orders::Step builtAt = Orders::NO_STEP;
};

struct StripShape {
	float g = 0;                     // the strip's half width
	float radius = 0;                // of a search circle, the units' movement since the build included
	float spacing = 0;               // between circle centres, by distance along the route
	int32_t directPerCircle = 0;     // at most this many enemies per circle: take them all
};
// The units that may lie within g of the route ahead of (ax, az), each once. The caller keeps those alive, near
// (squared distance to a circle centre or RouteNear, at their place now) and seen now, and sorts them by id.
// `visited` is the caller's scratch.
void StripCandidates(const Route& route, float ax, float az, const StripShape& shape, const SeenCache& seen,
                     int32_t ally, std::vector<int64_t>& visited, std::vector<Orders::UnitId>& out);

} // namespace KT1::Core::AMove

#endif
