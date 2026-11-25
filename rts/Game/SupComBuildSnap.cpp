/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
#include "SupComBuildSnap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/UnitDef.h"
#include "System/SpringMath.h"

#include <algorithm>
#include <cstdint>

bool SupComBuildSnap::Fine(const UnitDef* def)
{
	if (def == nullptr)
		return false;
	const auto it = def->customParams.find("supcom_footprint_squares");
	return (it != def->customParams.end() && it->second == "1");
}

float SupComBuildSnap::Axis(float v, int size)
{
	// BLD-001 v10 R10: the footprint low edge snaps to the nearest square line; an edge exactly halfway goes to the even line
	const int n = std::max(size, 1);
	const float e = v / SQUARE_SIZE - n * 0.5f;
	const float f = math::floor(e);
	const float r = e - f;
	float line;
	if (r == 0.5f)
		line = ((static_cast<int64_t>(f) & 1) == 0) ? f : f + 1.0f;
	else
		line = (r < 0.5f) ? f : f + 1.0f;
	return (line + n * 0.5f) * SQUARE_SIZE;
}
