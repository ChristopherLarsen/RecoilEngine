/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
#pragma once

struct UnitDef;

namespace SupComBuildSnap {
	// customParams.supcom_footprint_squares = "1": the footprint is in map squares and snaps to the 8-elmo grid
	bool Fine(const UnitDef* def);
	// the centre on one axis for a fine footprint of `size` squares: odd sizes centre on a square, even sizes on a square
	// line, the nearest one, ties up (the game snap, code/gamedata/place_rules.lua SnapAxis with the 8-elmo grid)
	float Axis(float v, int size);
}
