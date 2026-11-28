/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the GL half of SiteGhost.h (the draw functions of LuaUI/lib/site_ghost.lua). World phase; each call puts
// the GL state back. The site outline of a placement is the build grid's and not drawn here.

#ifndef KT1_SITE_GHOST_DRAW_H
#define KT1_SITE_GHOST_DRAW_H

#include <cstdint>
#include <span>

#include "Game/KT1/Draw/FxBuild/SiteGhost.h"

namespace KT1::FxBuild {

// a translucent finished model of the def in a tint; scale 1: full size
void DrawSiteModel(int32_t defId, int32_t team, const float (&rgb)[3], float alpha, float x, float y, float z,
	int32_t facing, float scale);
// a structure's footprint outlined; linkRgb: filled and drawn in the link's colour (nullptr: a plain neighbour)
void DrawNeighbour(const SiteGhostConfig& c, float x, float y, float z, float width, float depth, const float* linkRgb);
void DrawReach(const SiteGhostConfig& c, std::span<const Segment> segments);

} // namespace KT1::FxBuild

#endif
