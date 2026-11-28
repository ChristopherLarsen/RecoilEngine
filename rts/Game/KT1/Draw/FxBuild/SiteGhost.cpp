/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxBuild: the site ghost rules. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/SiteGhost.h"

namespace KT1::FxBuild {

float GhostAlpha(const SiteGhostConfig&, float) { return 0.0f; }
void Footprint(const SiteGhostConfig&, int32_t, int32_t, int32_t, float& width, float& depth) { width = 0.0f; depth = 0.0f; }
SiteStyle ActiveStyle(const SiteGhostConfig&, bool) { return {}; }
SiteStyle QueuedStyle(const SiteGhostConfig&, bool, bool) { return {}; }
SiteStyle ActiveSiteStyle(const SiteGhostConfig&, float) { return {}; }
void StartTimes(float, float, float, float, std::span<const QueuedWork>, std::vector<float>&) {}
std::string StartLabel(float) { return {}; }
void Neighbours(const SiteGhostConfig&, std::span<const SiteAt>, std::span<const Structure>, std::vector<int32_t>&) {}
bool InReachOfAny(float, float, float, float, std::span<const Reach>) { return false; }
void ReachArcs(const SiteGhostConfig&, std::span<const Reach>, std::vector<Segment>&) {}

} // namespace KT1::FxBuild
