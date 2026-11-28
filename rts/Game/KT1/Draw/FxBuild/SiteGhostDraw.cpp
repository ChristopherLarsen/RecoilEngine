/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the site ghost drawing. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/SiteGhostDraw.h"

namespace KT1::FxBuild {

void DrawSiteModel(int32_t, int32_t, const float (&)[3], float, float, float, float, int32_t, float) {}
void DrawNeighbour(const SiteGhostConfig&, float, float, float, float, float, const float*) {}
void DrawReach(const SiteGhostConfig&, std::span<const Segment>) {}

} // namespace KT1::FxBuild
