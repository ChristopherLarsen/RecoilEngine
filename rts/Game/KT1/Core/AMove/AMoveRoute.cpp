/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove core: stubs behind AMoveRoute.h (LUA-FREE H01; step S03 ports them).

#include "AMoveRoute.h"

namespace KT1::Core::AMove {

void BuildRoute(Route&, float, float, std::span<const Point>, float, float) {}
float SegmentDistSq(float, float, float, float, float, float) { return 0; }
float RouteDistSq(const Route&, float, float, float, float) { return 0; }
bool RouteNear(Route&, float, float, float, float, float) { return false; }
void AdvanceLeg(Route&, float, float) {}

void SeenCache::Init(int32_t, int32_t, float) {}
void SeenCache::Refresh(Orders::Step, Orders::Step) {}
bool SeenCache::Built(int32_t) const { return false; }
void SeenCache::Build(int32_t, std::span<const SeenUnit>) {}
std::span<const Orders::UnitId> SeenCache::All(int32_t) const { return {}; }
void SeenCache::Cells(int32_t, float, float, float, std::vector<int64_t>&, std::vector<Orders::UnitId>&) const {}

void StripCandidates(const Route&, float, float, const StripShape&, const SeenCache&, int32_t, std::vector<int64_t>&,
                     std::vector<Orders::UnitId>&) {}

} // namespace KT1::Core::AMove
